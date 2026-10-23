"""End-to-end check of the iPad remote, against a running game.

    python Tools/vibh2o_remote_check.py [--host 127.0.0.1] [--port 8642]

Plays the iPad's part: loads the page, reads the state, calls an attractor,
changes its mind half way, releases it, slides and switches an exclusion zone,
and checks at each step that Unreal answered - and HOW the pull moved: a
gradual rise, a turn with no jump, an exact return to rest.

Launch the demo map first, in Play or as a game, with the built-in simulator
running (the demo map has one), e.g.:

    UnrealEditor-Cmd.exe <repo>/VIBH2O_UE.uproject /Game/Maps/VibH2O_Demo -game -nullrhi -unattended

Standard library only.
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.request

SAMPLE_SECONDS = 0.1


class Remote:
    def __init__(self, host, port):
        self.base = f'http://{host}:{port}'

    def get(self, path):
        with urllib.request.urlopen(self.base + path, timeout=3) as response:
            return response.status, response.headers.get('Content-Type', ''), response.read()

    def state(self):
        return json.loads(self.get('/api/state')[2].decode('utf-8'))

    def send(self, *lines):
        body = '\n'.join(lines).encode('utf-8')
        request = urllib.request.Request(self.base + '/api/cmd', data=body, method='POST',
                                         headers={'Content-Type': 'text/plain'})
        with urllib.request.urlopen(request, timeout=3) as response:
            return response.read().decode('utf-8')


failures = []


def check(label, condition, detail=''):
    mark = 'OK   ' if condition else 'ECHEC'
    print(f'  [{mark}] {label}' + (f' - {detail}' if detail else ''))
    if not condition:
        failures.append(label)


def find(entries, ident):
    return next((e for e in entries if e['id'] == ident), None)


def sample_until(remote, ident, predicate, timeout):
    """Samples one attractor's pull until predicate(weight) holds."""
    samples = []
    start = time.monotonic()
    while time.monotonic() - start < timeout:
        attractor = find(remote.state()['attractors'], ident)
        samples.append((time.monotonic() - start, attractor['w']))
        if predicate(attractor['w']):
            break
        time.sleep(SAMPLE_SECONDS)
    return samples


def largest_jump(samples):
    return max((abs(b[1] - a[1]) for a, b in zip(samples, samples[1:])), default=0.0)


def main():
    parser = argparse.ArgumentParser(description='Controle de bout en bout de la telecommande iPad.')
    parser.add_argument('--host', default='127.0.0.1')
    parser.add_argument('--port', type=int, default=8642)
    parser.add_argument('--wait', type=float, default=90.0, help='attente maximale du jeu, en secondes')
    args = parser.parse_args()
    remote = Remote(args.host, args.port)

    print(f'Telecommande sur {remote.base}')

    # --- The game is up, the stage publishes, the room exists.
    deadline = time.monotonic() + args.wait
    state = None
    while time.monotonic() < deadline:
        try:
            state = remote.state()
            if state.get('attractors') and state.get('bubbles', 0) > 0:
                break
        except OSError:
            pass
        time.sleep(0.5)
    if not state or not state.get('attractors'):
        print('  [ECHEC] aucun etat publie : le jeu tourne-t-il, avec la carte de demo ?')
        return 1

    print('\n1. La page')
    status, content_type, page = remote.get('/')
    check('la page se charge', status == 200 and b'VIBH2O' in page, f'{len(page)} octets')
    check('servie en HTML', content_type.startswith('text/html'), content_type)
    for name in ('manifest.webmanifest', 'icon-180.png', 'icon-512.png'):
        status, _, body = remote.get('/' + name)
        check(f'{name} est servi', status == 200 and len(body) > 100)

    print('\n2. L\'etat')
    check('des bulles', state['bubbles'] > 0, f"{state['bubbles']} bulles")
    names = ', '.join(a['id'] for a in state['attractors'])
    check('des attracteurs', len(state['attractors']) >= 1, names)
    check('des zones', len(state.get('zones', [])) >= 1, ', '.join(z['id'] for z in state.get('zones', [])))

    attractor = state['attractors'][0]['id']
    remote.send('/Attractors/Release/', '/Attractors/FadeScale/ 1')
    sample_until(remote, attractor, lambda w: w <= 0.0, 20.0)

    print(f'\n3. Appel de l\'attracteur {attractor}')
    t0 = time.monotonic()
    check('commande acceptee', remote.send(f'/Attractor/{attractor}/ 1') == 'ok 1')
    rise = sample_until(remote, attractor, lambda w: w >= 0.98, 20.0)
    half = next((t for t, w in rise if w >= 0.5), None)
    check('l\'attracteur arrive', rise[-1][1] >= 0.98, f'{rise[-1][1]:.3f} en {time.monotonic() - t0:.1f} s')
    check('en douceur, pas d\'un coup', half is not None and half > 0.5, f'mi-chemin a {half:.2f} s' if half else '')
    check('sans jamais reculer', all(b[1] >= a[1] - 0.01 for a, b in zip(rise, rise[1:])))
    check('sans saut', largest_jump(rise) < 0.25, f'plus grand pas {largest_jump(rise):.3f} en {SAMPLE_SECONDS} s')

    print('\n4. Contre-ordre a mi-course')
    remote.send(f'/Attractor/{attractor}/ 0')
    sample_until(remote, attractor, lambda w: w <= 0.0, 20.0)
    remote.send(f'/Attractor/{attractor}/ 1')
    sample_until(remote, attractor, lambda w: w >= 0.45, 20.0)
    remote.send(f'/Attractor/{attractor}/ 0')
    turn = sample_until(remote, attractor, lambda w: w <= 0.0, 20.0)
    peak = max(w for _, w in turn)
    check('la bulle finit son elan puis revient', peak >= turn[0][1] - 0.01, f'pic {peak:.3f}')
    check('demi-tour sans saut', largest_jump(turn) < 0.25, f'plus grand pas {largest_jump(turn):.3f}')
    check('retour exact au repos', turn[-1][1] == 0.0, f'{turn[-1][1]}')

    print('\n5. Duree des fondus')
    remote.send('/Attractors/FadeScale/ 2')
    time.sleep(0.4)
    check('multiplicateur pris en compte', abs(remote.state()['fadeScale'] - 2.0) < 1e-3)
    remote.send('/Attractors/FadeScale/ 1')

    print('\n6. Tout relacher')
    remote.send(*[f"/Attractor/{a['id']}/ 1" for a in state['attractors']])
    time.sleep(1.0)
    remote.send('/Attractors/Release/')
    time.sleep(0.4)
    targets = [a['on'] for a in remote.state()['attractors'] if a['osc']]
    check('toutes les cibles a zero', all(t == 0 for t in targets), str(targets))

    zones = state.get('zones', [])
    if zones:
        zone = zones[0]['id']
        print(f'\n7. Zone d\'exclusion {zone}')
        remote.send(f'/Exclusion/{zone}/Position/ 0.5 -0.25')
        time.sleep(0.4)
        moved = find(remote.state()['zones'], zone)
        check('deplacee', abs(moved['x'] - 0.5) < 1e-3 and abs(moved['y'] + 0.25) < 1e-3, f"x {moved['x']} y {moved['y']}")
        remote.send(f'/Exclusion/{zone}/ 0')
        time.sleep(3.0)
        off = find(remote.state()['zones'], zone)
        check('eteinte en fondu', off['on'] == 0 and off['w'] == 0.0, f"poids {off['w']}")
        remote.send(f'/Exclusion/{zone}/ 1', f'/Exclusion/{zone}/Position/ 0 0')
        time.sleep(3.0)
        on = find(remote.state()['zones'], zone)
        check('rallumee', on['on'] == 1 and on['w'] > 0.99, f"poids {on['w']}")

    print('\n8. Robustesse')
    check('commande inconnue sans effet de bord', remote.send('/Attractor/NExistePas/ 1', 'pas une commande') == 'ok 1')
    try:
        remote.get('/../../Config/DefaultGame.ini')
        check('aucun fichier hors de la page', False)
    except urllib.error.HTTPError as error:
        check('aucun fichier hors de la page', error.code == 404, str(error.code))

    print()
    if failures:
        print(f'{len(failures)} echec(s) : ' + '; '.join(failures))
        return 1
    print('Tout est conforme.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
