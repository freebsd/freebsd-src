'''
Created on Jan 25, 2010

@author: tsitkova
'''
import os
import sys
import re
import yaml
from optparse import OptionParser
from confparser import ConfParser

class Rule(object):
    def __init__(self):
        pass
    
    def validate(self,node):
        (path,dirs,avs) = node        


class Validator(object):
    def __init__(self, kerberosPath, confPath=None, rulesPath=None, hfilePath=None):
        self.parser = ConfParser(kerberosPath)
        if confPath is not None:
            content = self._readConfigFile(confPath)
            rulesPath = content['RulesPath']
            hfilePath = content['HfilePath']
        if rulesPath is not None and hfilePath is not None:    
            self.rules = self._loadRules(rulesPath)
            self.validKeys = SupportedKeys(hfilePath).validKeys.union(self.rules['Attributes'])
        else:
            raise ValueError('Invalid arguments for validator: no path to rules and definition files')
        
        self._attribute_pattern = re.compile(r'^\w+$')
        self._lowercase_pattern = re.compile(r'[a-z]')

    def _readConfigFile(self,path):
        f = open(path)
        result = dict()
        for line in f:
            line = line.rstrip()
            fields = line.split('=')
            result[fields[0]] = fields[1]
        
        return result
    
    def _loadRules(self, path):
        f = open(path)
        rules = yaml.load(f)
        f.close()
        
        return rules
        
    def validate(self):
        typeInfo = self.rules['Types']
        
        for node in self.parser.walk():
            self._validateTypes(node, typeInfo)
            self._validateAttrubutes(node, self.validKeys)
            # self._validateRealm(node)


    def _validateTypes(self, node, typeInfo):
        (path, dirs, avs) = node
        for (key, value) in avs:
            valid_type_pattern = typeInfo.get(key)
            if valid_type_pattern is not None:
                for t in value:
                    if re.match(valid_type_pattern, t) is None:
                        print 'Wrong type %s for attribute %s.%s' % (t,path,key)
                        
    def _validateAttrubutes(self, node, validKeys):
        (path, dirs, avs) = node 
        attributes = list()
        for attr in dirs:
            if self._attribute_pattern.match(attr) is not None:
                attributes.append(attr)
        for (attr, value) in avs:
            if self._attribute_pattern.match(attr) is not None:
                attributes.append(attr)

        for attr in attributes:
            if attr not in validKeys:
                print 'Unrecognized attribute %s at %s' % (attr, path)

#    def _validateRealm(self, node):
#        (path, dirs, avs) = node 
#        if path == 'root.realms':
#            for attr in dirs:
#                if self._lowercase_pattern.search(attr) is not None:
#                    print 'Lower case letter in realm attribute: %s at %s' % (attr, path)

class SupportedKeys(object):
    def __init__(self, path):
        self.validKeys = self.getKeysFromHfile(path)
    
    def getKeysFromHfile(self, path):
        pattern = re.compile(r'^[#]define KRB5_CONF_\w+\s+["](\w+)["]')
        f = open(path)
        result = set()
        for l in f:
            l = l.rstrip()
            m = pattern.match(l)
            if m is not None:
                result.add(m.groups()[0])
        f.close()
        
        return result

    
class ValidatorTest(Validator):
    def __init__(self):
        self.kerberosPath = '../tests/kdc1.conf'
        self.rulesPath = '../tests/rules.yml'
        self.hfilePath = '../tests/k5-int.h'
        self.confPath = '../tests/validator.conf'

        super(ValidatorTest, self).__init__(self.kerberosPath, 
                                            rulesPath=self.rulesPath, 
                                            hfilePath=self.hfilePath)

    def run_tests(self):
        self._test_validate()
    
    def _test__loadRules(self):
        result = self._loadRules(self.rulesPath)
        print result
        
    def _test_validate(self):
        self.validate()
        
    def _test__readConfigFile(self):
        result = self._readConfigFile(self.confPath)
        print result

class SupportedKeysTest(SupportedKeys):
    def __init__(self):
        self.path = '../tests/k5-int.h'
        
    def run_tests(self):
        self._test_getKeysFromHFile()
    
    def _test_getKeysFromHFile(self):
        result = set()
        krb5keys = self.getKeysFromHfile(self.path)
        for key in krb5keys:
            print key
            result.update(key)
        print len(krb5keys)  
        
        return result
    
def _test():        
    tester = ValidatorTest()
    krb5keys = tester.run_tests()
                    
if __name__ == '__main__':
    TEST = False
    if TEST:
        _test()
        sys.exit()
    
    
    usage = "\n\t%prog path [-d defPath] [-r rulesPath] [-c validatorConfPath]"
    description = 'Description: validates kerberos configuration file'
    parser = OptionParser(usage = usage, description = description)
    parser.add_option("-c", dest="confPath",
                      help='path to validator config file')
    parser.add_option("-d", dest="hfilePath",
                      help='path to h-file with attribute definition')
    parser.add_option("-r", dest="rulesPath",
                      help='path to file with validation rules')
    (options, args) = parser.parse_args()

    if len(args) != 1 and len(sys.argv) <= 3:
        print '\n%s' % parser.get_usage()
        sys.exit()

    validator = None
    if options.confPath is not None:
        validator = Validator(args[0], confPath=options.confPath)
    elif options.hfilePath is not None and options.rulesPath is not None:
        validator = Validator(args[0], hfilePath=options.hfilePath, rulesPath=options.rulesPath)
    else:
        print '\nMust specify either configuration file or paths to rules and definitions files'
        print '%s' % parser.get_usage()
        sys.exit()
    
    validator.validate()
        


        
        
