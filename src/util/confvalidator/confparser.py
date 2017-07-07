'''
Created on Jan 31, 2010

@author: tsitkova
'''
import re
import copy
import yaml

class ConfParser(object):
    def __init__(self, path):
        self.configuration = self._parse(path)
    
    def walk(self):
        for trio in self._walk(self.configuration):
            yield trio
    
    def _parse(self, path):
        comment_pattern = re.compile(r'(\s*[#].*)')
        section_pattern = re.compile(r'^\s*\[(?P<section>\w+)\]\s+$')
        empty_pattern = re.compile(r'^\s*$')
        equalsign_pattern = re.compile(r'=')
        
        section = None
        parser_stack = list()
        result = dict()
        value = None
        f = open(path, 'r')
        for (ln,line) in enumerate(f):
            line = comment_pattern.sub('', line)
            line = equalsign_pattern.sub(' = ',line,count=1)
            if empty_pattern.match(line) is not None:
                continue
            m = section_pattern.match(line)
            if m is not None:
                section = m.group('section')
                value = dict()
                result[section] = value
                continue
            if section is None:
                msg = 'Failed to determine section for line #%i' % ln
                raise ValueError(msg)
            try:
                value = self._parseLine(value, line, parser_stack)
            except:
                print 'Error while parsing line %i: %s' % (ln+1, line)
                raise
        f.close()

        if len(parser_stack):
            raise 'Parsing error.'
        
        return result

    def _parseLine(self, value, content, stack):
        token_pattern = re.compile(r'(?P<token>\S+)(?=\s+)')
        attr = None
        token_stack = list()
        
        for m in token_pattern.finditer(content):
            token = m.group('token')
            if not self._validate(token):
                raise ValueError('Invalid token %s' % token)
            if token == '=':
                if len(token_stack) == 0:
                    raise ValueError('Failed to find attribute.')
                elif len(token_stack) == 1:
                    attr = token_stack.pop()
                else:
                    value[attr] = token_stack[:-1]
                    attr = token_stack[-1]
                    token_stack = list()
            elif token == '{':
                if attr is None:
                    raise ValueError('Failed to find attribute.')
                stack.append((attr,value))
                value = dict()
            elif token == '}':
                if len(stack) == 0:
                    raise ValueError('Failed to parse: unbalanced braces')
                if len(token_stack):
                    if attr is None:
                        raise ValueError('Missing attribute')
                    value[attr] = token_stack
                    attr = None
                    token_stack = list()
                (attr,parent_value) = stack.pop()                                
                parent_value[attr] = value
                value = parent_value
            else:
                token_stack.append(token)
        if len(token_stack):
            if attr is None:
                raise ValueError('Missing attribute')
            value[attr] =  token_stack
                
        return value

    def _validate(self, token):
        result = True
        for s in ['{','}']:
            if s in token and s != token:
                result = False
        
        return result

    def _walk(self, parsedData, path='root'):
        dirs = list()
        av = list()
        for (key, value) in parsedData.iteritems():
            if type(value) == dict:
                new_path = path + '.' + key
                for trio in self._walk(value, new_path):
                    yield trio
                dirs.append(key)
            else:
                av.append((key,value))
        yield (path, dirs, av)



class ConfParserTest(ConfParser):
    def __init__(self):
        self.conf_path = '../tests/krb5.conf'
        super(ConfParserTest, self).__init__(self.conf_path)

    def run_tests(self):
        self._test_walk()
            
    def _test_parse(self):
        result = self._parse(self.conf_path)
        print yaml.dump(result)
        
    def _test_walk(self):
        configuration = self._parse(self.conf_path)
        for (path,dirs,av) in self.walk():
            print path,dirs,av



        
if __name__ == '__main__':
    tester = ConfParserTest()
    tester.run_tests()
