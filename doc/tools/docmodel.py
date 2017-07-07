'''
  Copyright 2011 by the Massachusetts
  Institute of Technology.  All Rights Reserved.

  Export of this software from the United States of America may
  require a specific license from the United States Government.
  It is the responsibility of any person or organization contemplating
  export to obtain such a license before exporting.

  WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
  distribute this software and its documentation for any purpose and
  without fee is hereby granted, provided that the above copyright
  notice appear in all copies and that both that copyright notice and
  this permission notice appear in supporting documentation, and that
  the name of M.I.T. not be used in advertising or publicity pertaining
  to distribution of the software without specific, written prior
  permission.  Furthermore if you modify this software you must label
  your software as modified software and not distribute it in such a
  fashion that it might be confused with the original M.I.T. software.
  M.I.T. makes no representations about the suitability of
  this software for any purpose.  It is provided "as is" without express
  or implied warranty.
'''
import re

from Cheetah.Template import Template

class Attribute(object):
    def __init__(self, **argkw):
        self.definition = argkw.get('definition')
        self.name = argkw.get('name')
        self.type = argkw.get('type')
        self.typeId = argkw.get('typeId')
        self.short_description = argkw.get('short_description')
        self.long_description = argkw.get('long_description')
        self.version = argkw.get('version')

    def __repr__(self):
        result = list()
        for (attr,value) in self.__dict__.iteritems():
            result.append('%s=%s' % (attr,value))
        return 'Attribute: %s' % ','.join(result)


class CompositeType():
    def __init__(self, **argkw):
        self.category = 'composite'
        self.definition = argkw.get('definition')
        self.name = argkw.get('name')
        self.name_signature = argkw.get('name_signature')
        self.Id = argkw.get('Id')
        self.initializer = argkw.get('initializer')
        self.active = argkw.get('active', False)
        self.version = argkw.get('version')
        self.return_type = argkw.get('return_type')
        self.short_description = argkw.get('short_description')
        self.long_description = argkw.get('long_description')
        self.friends = argkw.get('friends')
        self.type = argkw.get('type')
        self.attributes = self._setAttributes(argkw.get('attributes'))

    def __repr__(self):
        result = list()
        for (attr,value) in self.__dict__.iteritems():
            if attr == 'attributes':
                if value is not None:
                    attributes = ['%s' % a for a in value]
                    value = '\n  %s' % '\n  '.join(attributes)

            result.append('%s: %s' % (attr,value))
        result = '\n'.join(result)

        return result

    def _setAttributes(self, attributes):
        result = None
        if attributes is not None:
            result = list()
            for a in attributes:
                result.append(Attribute(**a))

        return result

    def struct_reference(self, name):
        result = re.sub(r'_', '-', name)
        result = '_%s-struct' % result

        return result

    def macro_reference(self, name):
        result = re.sub(r'_', '-', name)
        result = '_%s-data' % result

        return result

class Parameter(object):
    def __init__(self, **argkw):
        self.seqno = argkw.get('seqno')
        self.name = argkw.get('name')
        self.direction = argkw.get('direction')
        self.type = argkw.get('type')
        self.typeId = argkw.get('typeId')
        self.description = argkw.get('description')
        self.version = argkw.get('version')

    def __repr__(self):
        content = (self.name,self.direction,self.seqno,self.type,self.typeId,self.description)
        return 'Parameter: name=%s,direction=%s,seqno=%s,type=%s,typeId=%s,descr=%s' % content

class Function(object):
    def __init__(self, **argkw):
        self.category = 'function'
        self.name = argkw.get('name')
        self.Id = argkw.get('Id')
        self.active = argkw.get('active', False)
        self.version = argkw.get('version')
        self.parameters = self._setParameters(argkw.get('parameters'))
        self.return_type = argkw.get('return_type')
        self.return_description = argkw.get('return_description')
        self.retval_description = argkw.get('retval_description')
        self.warn_description = argkw.get('warn_description')
        self.sa_description = argkw.get('sa_description')
        self.notes_description = argkw.get('notes_description')
        self.version_num = argkw.get('version_num')
        self.short_description = argkw.get('short_description')
        self.long_description = argkw.get('long_description')
        self.deprecated_description = argkw.get('deprecated_description')
        self.friends = argkw.get('friends')

    def _setParameters(self, parameters):
        result = None
        if parameters is not None:
            result = list()
            for p in parameters:
                result.append(Parameter(**p))

        return result

    def getObjectRow(self):
        result = [str(self.Id),
                  self.name,
                  self.category]

        return ','.join(result)

    def getObjectDescriptionRow(self):
        result = [self.Id,
                  self.active,
                  self.version,
                  self.short_description,
                  self.long_description]

        return ','.join(result)

    def getParameterRows(self):
        result = list()
        for p in self.parameters:
            p_row = [self.Id,
                     p.name,
                     p.seqno,
                     p.type,
                     p.typeId,
                     p.description,
                     p.version]
            result.append(','.join(p_row))

        return '\n'.join(result)

    def __repr__(self):
        lines = list()
        lines.append('Category: %s' % self.category)
        lines.append('Function name: %s' % self.name)
        lines.append('Function Id: %s' % self.Id)
        parameters = ['  %s' % p for p in self.parameters]
        lines.append('Parameters:\n%s' % '\n'.join(parameters))
        lines.append('Function return type: %s' % self.return_type)
        lines.append('Function return type description:\n%s' % self.return_description)
        lines.append('Function retval description:\n%s' % self.retval_description)
        lines.append('Function short description:\n%s' % self.short_description)
        lines.append('Function long description:\n%s' % self.long_description)
        lines.append('Warning description:\n%s' % self.warn_description)
        lines.append('See also description:\n%s' % self.sa_description)
        lines.append('NOTE description:\n%s' % self.notes_description)
        lines.append('Version introduced:\n%s' % self.version_num)
        lines.append('Deprecated description:\n%s' % self.deprecated_description)
        result = '\n'.join(lines)

        return result


class DocModel(object):
    def __init__(self, **argkw):
        if len(argkw):
            self.name = argkw['name']
            if argkw['category'] == 'function':
                self.category = 'function'
                self.function = Function(**argkw)
            elif argkw['category'] == 'composite':
                self.category = 'composite'
                self.composite = CompositeType(**argkw)

    def __repr__(self):
        obj = getattr(self,self.category)
        print type(obj)
        return str(obj)

    def signature(self):
        param_list = list()
        for p in self.function.parameters:
            if p.type is "... " :
                param_list.append('%s %s' % (p.type,' '))
            else:
                param_list.append('%s %s' % (p.type, p.name))
        param_list = ', '.join(param_list)
        result = '%s %s(%s)' % (self.function.return_type,
                                self.function.name, param_list)

        return result

    def save(self, path, template_path):
        f = open(template_path, 'r')
        t = Template(f.read(),self)
        out = open(path, 'w')
        out.write(str(t))
        out.close()
        f.close()


class DocModelTest(DocModel):
    def __init__(self):
        doc_path = '../docutil/example.yml'
        argkw = yaml.load(open(doc_path,'r'))
        super(DocModelTest,self).__init__(**argkw)

    def run_tests(self):
        self.test_save()

    def test_print(self):
        print 'testing'
        print self


    def test_save(self):
        template_path = '../docutil/function2edit.html'

        path = '/var/tsitkova/Sources/v10/trunk/documentation/test_doc.html'

        self.save(path, template_path)

if __name__ == '__main__':
    tester = DocModelTest()
    tester.run_tests()
