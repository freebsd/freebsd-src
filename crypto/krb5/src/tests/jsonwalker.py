import sys
import json
from collections import defaultdict
from optparse import OptionParser

class Parser(object):
    DEFAULTS = {int:0,
                str:'',
                list:[]}

    def __init__(self, defconf=None):
        self.defaults = None
        if defconf is not None:
            self.defaults = self.flatten(defconf)

    def run(self, logs, verbose=None):
        result = self.parse(logs)
        if len(result) != len(self.defaults):
            diff = set(self.defaults.keys()).difference(result.keys())
            print('Test failed.')
            print('The following attributes were not set:')
            for it in diff:
                print(it)
            sys.exit(1)

    def flatten(self, defaults):
        """
        Flattens paths to attributes.

        Parameters
        ----------
        defaults : a dictionaries populated with default values

        Returns :
        dict : with flattened attributes
        """
        result = dict()
        for path,value in self._walk(defaults):
            if path in result:
                print('Warning: attribute path %s already exists' % path)
            result[path] = value

        return result

    def parse(self, logs):
        result = defaultdict(list)
        for msg in logs:
            # each message is treated as a dictionary of dictionaries
            for a,v in self._walk(msg):
                # see if path is registered in defaults
                if a in self.defaults:
                    dv = self.defaults.get(a)
                    if dv is None:
                        # determine default value by type
                        if v is not None:
                            dv = self.DEFAULTS[type(v)]
                        else:
                            print('Warning: attribute %s is set to None' % a)
                            continue
                    # by now we have default value
                    if v != dv:
                        # test passed
                        result[a].append(v)
        return result

    def _walk(self, adict):
        """
        Generator that works through dictionary.
        """
        for a,v in adict.items():
            if isinstance(v,dict):
                for (attrpath,u) in self._walk(v):
                    yield (a+'.'+attrpath,u)
            else:
                yield (a,v)


if __name__ == '__main__':

    parser = OptionParser()
    parser.add_option("-i", "--logfile", dest="filename",
                  help="input log file in json fmt", metavar="FILE")
    parser.add_option("-d", "--defaults", dest="defaults",
                  help="dictionary with defaults", metavar="FILE")

    (options, args) = parser.parse_args()
    if options.filename is not None:
        with open(options.filename, 'r') as f:
            content = list()
            for l in f:
                content.append(json.loads(l.rstrip()))
        f.close()
    else:
        print('Input file in JSON format is required')
        exit()

    defaults = None
    if options.defaults is not None:
        with open(options.defaults, 'r') as f:
            defaults = json.load(f)

    # run test
    p = Parser(defaults)
    p.run(content)
    exit()
