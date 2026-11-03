#!/usr/bin/env python3
"""Read-only Volumio check. Does not start/stop audio, scan or change settings."""
import argparse
import json
import re
import urllib.parse
import urllib.request

def probe(host, port=80):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9.-]{0,62}', host) or not 1 <= port <= 65535:
        raise ValueError('IP/Hostname ohne http:// und Port 1..65535 verwenden')
    paths = {
        'state': '/api/v1/getState',
        'queue': '/api/v1/getQueue',
        'playlists': '/api/v1/listplaylists',
        'dab': '/api/v1/browse?' + urllib.parse.urlencode({'uri': 'rtlsdr://dab?view=flat'}),
    }
    result = {}
    for name, path in paths.items():
        try:
            with urllib.request.urlopen(f'http://{host}:{port}{path}', timeout=4) as response:
                data = response.read(512*1024+1)
                if len(data) > 512*1024:
                    raise ValueError('Antwort zu gross')
                result[name] = json.loads(data)
        except Exception as exc:
            result[name] = {'probe_error': str(exc)}
    return result

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('host'); parser.add_argument('--port', type=int, default=80)
    args=parser.parse_args()
    print(json.dumps(probe(args.host, args.port), ensure_ascii=False, indent=2))
