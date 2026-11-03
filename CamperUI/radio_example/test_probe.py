import importlib.util
import json
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('probe',Path(__file__).with_name('probe_volumio.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ProbeTest(unittest.TestCase):
    def test_read_only_routes(self):
        # In-process fake HTTP response: no sockets or real player required.
        from unittest.mock import patch
        class Response:
            def __init__(self,url):self.url=url
            def __enter__(self):return self
            def __exit__(self,*args):pass
            def read(self,limit):return json.dumps({'url':self.url}).encode()
        urls=[]
        def open_url(url,timeout):urls.append(url);return Response(url)
        with patch.object(module.urllib.request,'urlopen',side_effect=open_url):
            result=module.probe('volumio.local')
        self.assertEqual(set(result),{'state','queue','playlists','dab'})
        self.assertEqual(len(urls),4)
        self.assertTrue(all('/commands/' not in u and 'rescan' not in u for u in urls))
        self.assertIn('uri=rtlsdr%3A%2F%2Fdab%3Fview%3Dflat',urls[-1])
    def test_partial_failure(self):
        from unittest.mock import patch
        with patch.object(module.urllib.request,'urlopen',side_effect=TimeoutError('offline')):
            result=module.probe('volumio.local')
        self.assertTrue(all('probe_error' in x for x in result.values()))
    def test_invalid_host(self):
        with self.assertRaises(ValueError):module.probe('http://not-a-host')
if __name__=='__main__':unittest.main()
