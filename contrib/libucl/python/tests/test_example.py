from .compat import unittest
import json
import ucl

_ucl_inp = '''
param = value;
section {
    param = value;
    param1 = value1;
    flag = true;
    number = 10k;
    time = 0.2s;
    string = "something";
    subsection {
        host = {
            host = "hostname";
            port = 900;
        }
        host = {
            host = "hostname";
            port = 901;
        }
    }
}
'''

_json_res = {
	'param': 'value',
	'section': {
		'param': 'value',
		'param1': 'value1',
		'flag': True,
		'number': 10000,
		'time': '0.2s',
		'string': 'something',
		'subsection': {
			'host': [
				{
					'host': 'hostname',
					'port': 900,
				},
				{
					'host': 'hostname',
					'port': 901,
				}
			]
		}
	}
}

class TestExample(unittest.TestCase):
	def test_example(self):
		# load in sample UCL
		u = ucl.load(_ucl_inp)

		# Output and read back the JSON
		uj = json.loads(json.dumps(u))

		self.assertEqual(uj, _json_res)
