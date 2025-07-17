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
import sys
import re

from collections import defaultdict
from xml.sax import make_parser
from xml.sax.handler import ContentHandler
from docmodel import *

exclude_funcs = ['krb5_free_octet_data']

class DocNode(object):
    """
    Represents the structure of xml node.
    """
    def __init__(self, name):
        """
        @param node: name - the name of a node.
        @param attributes: a dictionary populated with attributes of a node
        @param children: a dictionary with lists of children nodes. Nodes
            in lists are ordered as they appear in a document.
        @param content: a content of xml node represented as a list of
            tuples [(type,value)] with type = ['char'|'element'].
            If type is 'char' then the value is a character string otherwise
            it is a reference to a child node.
        """
        self.name = name
        self.content = list()
        self.attributes = dict()
        self.children = defaultdict(list)

    def walk(self, decorators, sub_ws, stack=[]):
        result = list()
        decorator = decorators.get(self.name, decorators['default'])
        stack.append(decorators['default'])
        decorators['default'] = decorator

        for (obj_type,obj) in self.content:
            if obj_type == 'char':
                if obj != '':
                    result.append(obj)
            else:
                partial = obj.walk(decorators,1, stack)
                if partial is not None:
                    result.append(' %s ' % partial)
        decorators['default'] = stack.pop()
        result = decorator(self, ''.join(result))
        if result is not None:
            if sub_ws == 1:
                result = re.sub(r'[ ]+', r' ', result)
            else:
                result = result.strip()

        return result

    def getContent(self):
        decorators = {'default': lambda node,value: value}
        result = self.walk(decorators, 1)
        if len(result) == 0:
            result = None

        return result

    def __repr__(self):
        result = ['Content: %s' % self.content]

        for (key,value) in self.attributes.iteritems():
            result.append('Attr: %s = %s' % (key,value))
        for (key,value) in self.children.iteritems():
            result.append('Child: %s,%i' % (key,len(value)))

        return '\n'.join(result)

class DoxyContenHandler(ContentHandler):
    def __init__(self, builder):
        self.builder = builder
        self.counters = defaultdict(int)
        self._nodes = None
        self._current = None

    def startDocument(self):
        pass

    def endDocument(self):
        import sys

    def startElement(self, name, attrs):
        if name == self.builder.toplevel:
            self._nodes = []

        if name == 'memberdef':
            kind = attrs.get('kind')
            if kind is None:
                raise ValueError('Kind is not defined')
            self.counters[kind] += 1

        if self._nodes is None:
            return

        node = DocNode(name)
        for (key,value) in attrs.items():
            node.attributes[key] = value
        if self._current is not None:
            self._current.children[name].append(node)
            self._nodes.append(self._current)
        self._current = node

    def characters(self, content):

        if self._current is not None:
            self._current.content.append(('char',content.strip()))

    def endElement(self, name):
        if name == self.builder.toplevel:
            assert(len(self._nodes) == 0)
            self._nodes = None
            self.builder.document.append(self._current)
            self._current = None
        else:
            if self._nodes is not None:
                node = self._current
                self._current = self._nodes.pop()
                self._current.content.append(('element',node))


class XML2AST(object):
    """
    Translates XML document into Abstract Syntax Tree like representation
    The content of document is stored in self.document
    """
    def __init__(self, xmlpath, toplevel='doxygen'):
        self.document = list()
        self.toplevel = toplevel
        self.parser = make_parser()
        handler = DoxyContenHandler(self)
        self.parser.setContentHandler(handler)
        filename = 'krb5_8hin.xml'
        filepath = '%s/%s' % (xmlpath,filename)
        self.parser.parse(open(filepath,'r'))


class DoxyFuncs(XML2AST):
    def __init__(self, path):
        super(DoxyFuncs, self).__init__(path,toplevel='memberdef')
        self.objects = list()

    def run(self):
        for node in self.document:
            self.process(node)

    def process(self, node):
        node_type = node.attributes['kind']
        if node_type == 'function':
            data = self._process_function_node(node)
        else:
            return

        if 'name' in data and data['name'] in exclude_funcs:
            return
        self.objects.append(DocModel(**data))

    def save(self, templates, target_dir):
        for obj in self.objects:
            template_path = templates[obj.category]
            outpath = '%s/%s.rst' % (target_dir,obj.name)
            obj.save(outpath, template_path)


    def _process_function_node(self, node):
        f_name = node.children['name'][0].getContent()
        f_Id = node.attributes['id']
        f_ret_type = self._process_type_node(node.children['type'][0])
        f_brief = node.children['briefdescription'][0].getContent()
        f_detailed = node.children['detaileddescription'][0]
        detailed_description = self._process_description_node(f_detailed)
        return_value_description = self._process_return_value_description(f_detailed)
        retval_description = self._process_retval_description(f_detailed)
        warning_description = self._process_warning_description(f_detailed)
        seealso_description = self._process_seealso_description(f_detailed)
        notes_description = self._process_notes_description(f_detailed)
        f_version = self._process_version_description(f_detailed)
        deprecated_description = self._process_deprecated_description(f_detailed)
        param_description_map = self.process_parameter_description(f_detailed)
        f_definition = node.children['definition'][0].getContent()
        f_argsstring = node.children['argsstring'][0].getContent()

        function_descr = {'category': 'function',
                          'name': f_name,
                          'Id': f_Id,
                          'return_type': f_ret_type[1],
                          'return_description': return_value_description,
                          'retval_description': retval_description,
                          'sa_description': seealso_description,
                          'warn_description': warning_description,
                          'notes_description': notes_description,
                          'short_description': f_brief,
                          'version_num': f_version,
                          'long_description': detailed_description,
                          'deprecated_description': deprecated_description,
                          'parameters': list()}

        parameters = function_descr['parameters']
        for (i,p) in enumerate(node.children['param']):
            type_node = p.children['type'][0]
            p_type = self._process_type_node(type_node)
            if p_type[1].find('...') > -1 :
                p_name = ''
            else:
                p_name = None
            p_name_node = p.children.get('declname')
            if p_name_node is not None:
                p_name = p_name_node[0].getContent()
            (p_direction,p_descr) = param_description_map.get(p_name,(None,None))

            param_descr = {'seqno': i,
                           'name': p_name,
                           'direction': p_direction,
                           'type': p_type[1],
                           'typeId': p_type[0],
                           'description': p_descr}
            parameters.append(param_descr)
        result = Function(**function_descr)
        print(result, file=self.tmp)

        return function_descr

    def _process_type_node(self, type_node):
        """
        Type node has form
            <type>type_string</type>
        for build in types and
            <type>
              <ref refid='reference',kindref='member|compound'>
                  'type_name'
              </ref></type>
              postfix (ex. *, **m, etc.)
            </type>
        for user defined types.
        """
        type_ref_node = type_node.children.get('ref')
        if type_ref_node is not None:
            p_type_id = type_ref_node[0].attributes['refid']
        else:
            p_type_id = None
        p_type = type_node.getContent()
        # remove some macros
        p_type = re.sub('KRB5_ATTR_DEPRECATED', '', p_type)
        p_type = re.sub('KRB5_CALLCONV_C', '', p_type)
        p_type = re.sub('KRB5_CALLCONV_WRONG', '', p_type)
        p_type = re.sub('KRB5_CALLCONV', '', p_type)
        p_type = p_type.strip()

        return (p_type_id, p_type)

    def _process_description_node(self, node):
        """
        Description node is comprised of <para>...</para> sections
        """
        para = node.children.get('para')
        result = list()
        if para is not None:
            decorators = {'default': self.paragraph_content_decorator}
            for e in para:
                result.append(str(e.walk(decorators, 1)))
                result.append('\n')
        result = '\n'.join(result)

        return result

    def return_value_description_decorator(self, node, value):
        if node.name == 'simplesect':
            if node.attributes['kind'] == 'return':
                cont = set()
                cont = node.getContent()
                return  value
        else:
            return None

    def paragraph_content_decorator(self, node, value):
        if node.name == 'para':
            return value + '\n'
        elif node.name == 'simplesect':
            if node.attributes['kind'] == 'return':
                return None
        elif node.name == 'ref':
            if value.find('()') >= 0:
                # functions
                return ':c:func:' + '`' + value + '`'
            else:
                # macro's
                return ':data:' + '`' + value + '`'
        elif node.name == 'emphasis':
            return '*' + value + '*'
        elif node.name == 'itemizedlist':
            return '\n' + value
        elif node.name == 'listitem':
            return '\n\t - ' + value + '\n'
        elif node.name == 'computeroutput':
            return '**' + value + '**'
        else:
            return None

    def parameter_name_decorator(self, node, value):
        if node.name == 'parametername':
            direction = node.attributes.get('direction')
            if direction is not None:
                value = '%s:%s' % (value,direction)
            return value

        elif node.name == 'parameterdescription':
            return None
        else:
            return value

    def parameter_description_decorator(self, node, value):
        if node.name == 'parameterdescription':
            return value
        elif node.name == 'parametername':
            return None
        else:
            return value

    def process_parameter_description(self, node):
        """
        Parameter descriptions reside inside detailed description section.
        """
        para = node.children.get('para')
        result = dict()
        if para is not None:
            for e in para:

                param_list = e.children.get('parameterlist')
                if param_list is None:
                    continue
                param_items = param_list[0].children.get('parameteritem')
                if param_items is None:
                    continue
                for it in param_items:
                    decorators = {'default': self.parameter_name_decorator}
                    direction = None
                    name = it.walk(decorators,0).split(':')
                    if len(name) == 2:
                        direction = name[1]

                    decorators = {'default': self.parameter_description_decorator,
                                  'para': self.paragraph_content_decorator}
                    description = it.walk(decorators, 0)
                    result[name[0]] = (direction,description)
        return result


    def _process_return_value_description(self, node):
        result = None
        ret = list()

        para = node.children.get('para')
        if para is not None:
            for p in para:
                simplesect_list = p.children.get('simplesect')
                if simplesect_list is None:
                    continue
                for it in simplesect_list:
                    decorators = {'default': self.return_value_description_decorator,
                                  'para': self.parameter_name_decorator}
                    result = it.walk(decorators, 1)
                    if result is not None:
                        ret.append(result)
        return ret


    def _process_retval_description(self, node):
        """
        retval descriptions reside inside detailed description section.
        """
        para = node.children.get('para')

        result = None
        ret = list()
        if para is not None:

            for e in para:
                param_list = e.children.get('parameterlist')
                if param_list is None:
                    continue
                for p in param_list:
                    kind = p.attributes['kind']
                    if kind == 'retval':

                        param_items = p.children.get('parameteritem')
                        if param_items is None:
                            continue


                        for it in param_items:
                            param_descr = it.children.get('parameterdescription')
                            if param_descr is not None:
                                val = param_descr[0].children.get('para')

                                if val is not None:
                                    val_descr = val[0].getContent()

                                else:
                                    val_descr =''

                            decorators = {'default': self.parameter_name_decorator}

                            name = it.walk(decorators, 1).split(':')

                            val = name[0]
                            result = " %s  %s" % (val, val_descr)
                            ret.append (result)
        return ret

    def return_warning_decorator(self, node, value):
        if node.name == 'simplesect':
            if node.attributes['kind'] == 'warning':
                return value
        else:
            return None

    def _process_warning_description(self, node):
        result = None
        para = node.children.get('para')
        if para is not None:
            for p in para:
                simplesect_list = p.children.get('simplesect')
                if simplesect_list is None:
                    continue
                for it in simplesect_list:
                    decorators = {'default': self.return_warning_decorator,
                                  'para': self.paragraph_content_decorator}
                    result = it.walk(decorators, 1)
                    # Assuming that only one Warning per function
                    if result is not None:
                        return result
        return result

    def return_seealso_decorator(self, node, value):
        if node.name == 'simplesect':
            if node.attributes['kind'] == 'see':
                return value
        else:
            return None

    def _process_seealso_description(self, node):
        result = None
        para = node.children.get('para')
        if para is not None:
            for p in para:
                simplesect_list = p.children.get('simplesect')
                if simplesect_list is None:
                    continue
                for it in simplesect_list:
                    decorators = {'default': self.return_seealso_decorator,
                                  'para': self.paragraph_content_decorator}
                    result = it.walk(decorators, 1)
        return result

    def return_version_decorator(self, node, value):
        if node.name == 'simplesect':
            if node.attributes['kind'] == 'version':
                return value
        else:
            return None

    def _process_version_description(self, node):
        result = None
        para = node.children.get('para')
        if para is not None:
            for p in para:
                simplesect_list = p.children.get('simplesect')
                if simplesect_list is None:
                    continue
                for it in simplesect_list:
                    decorators = {'default': self.return_version_decorator,
                                  'para': self.paragraph_content_decorator}
                    result = it.walk(decorators, 1)
                    if result is not None:
                        return result
        return result

    def return_notes_decorator(self, node, value):
        if node.name == 'simplesect':
            if node.attributes['kind'] == 'note':
                # We indent notes with an extra tab.  Do it for all paragraphs.
                return value.replace("\n  ", "\n\n\t  ");
        else:
            return None

    def _process_notes_description(self, node):
        result = None
        para = node.children.get('para')
        if para is not None:
            for p in para:
                simplesect_list = p.children.get('simplesect')
                if simplesect_list is None:
                    continue
                for it in simplesect_list:
                    decorators = {'default': self.return_notes_decorator,
                                  'para': self.paragraph_content_decorator}
                    result = it.walk(decorators, 1)
                    if result is not None:
                        return result
        return result

    def return_deprecated_decorator(self, node, value):
        if node.name == 'xrefsect':
            if node.attributes['id'].find('deprecated_') > -1:
                xreftitle = node.children.get('xreftitle')
                if xreftitle[0] is not None:
                    xrefdescr = node.children.get('xrefdescription')
                    deprecated_descr = "DEPRECATED %s" % xrefdescr[0].getContent()
                    return deprecated_descr
        else:
            return None

    def _process_deprecated_description(self, node):
        result = None
        para = node.children.get('para')
        if para is not None:
            for p in para:
                xrefsect_list = p.children.get('xrefsect')
                if xrefsect_list is None:
                    continue
                for it in xrefsect_list:
                    decorators = {'default': self.return_deprecated_decorator,
                                  'para': self.paragraph_content_decorator}
                    result = it.walk(decorators, 1)
                    if result is not None:
                        return result
        return result

    def break_into_lines(self, value, linelen=82):
        breaks = range(0,len(value),linelen) + [len(value)]
        result = list()
        for (start,end) in zip(breaks[:-1],breaks[1:]):
            result.append(value[start:end])
        result = '\n'.join(result)

        return result

    def _save(self, table, path = None):
        if path is None:
            f = sys.stdout
        else:
            f = open(path, 'w')
        for l in table:
            f.write('%s\n' % ','.join(l))
        if path is not None:
            f.close()



class DoxyBuilderFuncs(DoxyFuncs):
    def __init__(self, xmlpath, rstpath):
        super(DoxyBuilderFuncs,self).__init__(xmlpath)
        self.target_dir = rstpath
        outfile = '%s/%s' % (self.target_dir, 'out.txt')
        self.tmp = open(outfile, 'w')

    def run_all(self):
        self.run()
        templates = {'function': 'func_document.tmpl'}
        self.save(templates, self.target_dir)

    def test_run(self):
        self.run()

if __name__ == '__main__':
    builder = DoxyBuilderFuncs(xmlpath, rstpath)
    builder.run_all()

