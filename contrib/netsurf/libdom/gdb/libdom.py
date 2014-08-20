# LibDOM related commands and utilities for gdb

import gdb

def dom_get_type_ptr(typename):
    return gdb.lookup_type(typename).pointer()

def dom_node_at(ptr):
    nodetype = dom_get_type_ptr("dom_node_internal")
    return ptr.cast(nodetype).dereference()

def dom_document_at(ptr):
    doctype = dom_get_type_ptr("dom_document")
    return ptr.cast(doctype).dereference()

def dom_node_type(node):
    return node["type"]

def dom_node_refcnt(node):
    return node["base"]["refcnt"]

def lwc_string_value(strptr):
    cptr = strptr+1
    charptr = cptr.cast(dom_get_type_ptr("char"))
    return charptr.string()

def dom_string__is_intern(intstr):
    return str(intstr['type']) == "DOM_STRING_INTERNED"

def cdata_string_value(cdata):
    cptr = cdata['ptr']
    charptr = cptr.cast(dom_get_type_ptr("char"))
    return charptr.string()

def dom_string_value(stringptr):
    intstr = stringptr.cast(dom_get_type_ptr("dom_string_internal")).dereference()
    if intstr.address == gdb.parse_and_eval("(dom_string_internal*)0"):
        return ""
    if dom_string__is_intern(intstr):
        return lwc_string_value(intstr['data']['intern'])
    else:
        return cdata_string_value(intstr['data']['cdata'])

def dom_node_name(node):
    namestr = node["name"]
    return " '%s'" % dom_string_value(namestr)

def dom_node_pending_offset():
    return gdb.parse_and_eval("(int)&((struct dom_node_internal *)0)->pending_list")

def dom_print_node(node, prefix = ""):
    print("%s%s @ %s [%s]%s" % (prefix, dom_node_type(node), 
                                node.address, dom_node_refcnt(node),
                                dom_node_name(node)))

def dom_walk_tree(node, prefix = ""):
    dom_print_node(node, prefix)
    current = node['first_child'].dereference()
    while current.address != 0:
        dom_walk_tree(current, "%s  " % prefix)
        current = current['next'].dereference()

def dom_document_show(doc):
    print "Node Tree:"
    node = dom_node_at(doc.address)
    dom_walk_tree(node, "  ")
    pending = doc['pending_nodes']
    if pending['next'] != pending.address:
        print "Pending Node trees:"
        current_list_entry = pending['next']
        while current_list_entry is not None:
            voidp = current_list_entry.cast(dom_get_type_ptr("void"))
            voidp = voidp - dom_node_pending_offset()
            node = dom_node_at(voidp)
            dom_walk_tree(node, "  ")
            current_list_entry = node['pending_list']['next']
            if current_list_entry == pending.address:
                current_list_entry = None
    

class DOMCommand(gdb.Command):
    """DOM related commands"""

    def __init__(self):
        gdb.Command.__init__(self, "dom", gdb.COMMAND_DATA,
                             gdb.COMPLETE_COMMAND, True)

class DOMNodeCommand(gdb.Command):
    """DOMNode related commands"""

    def __init__(self):
        gdb.Command.__init__(self, "dom node", gdb.COMMAND_DATA,
                             gdb.COMPLETE_COMMAND, True)

class DOMDocumentCommand(gdb.Command):
    """DOMDocument related commands"""

    def __init__(self):
        gdb.Command.__init__(self, "dom document", gdb.COMMAND_DATA,
                             gdb.COMPLETE_COMMAND, True)

class DOMNodeShowCommand(gdb.Command):
    """Show a node at a given address."""

    def __init__(self):
        gdb.Command.__init__(self, "dom node show", gdb.COMMAND_DATA,
                             gdb.COMPLETE_NONE, True)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        self._invoke(*args)

    def _invoke(self, nodeptr):
        _ptr = gdb.parse_and_eval(nodeptr)
        node = dom_node_at(_ptr)
        dom_print_node(node)

class DOMNodeWalkCommand(gdb.Command):
    """Walk a node tree at a given address."""

    def __init__(self):
        gdb.Command.__init__(self, "dom node walk", gdb.COMMAND_DATA,
                             gdb.COMPLETE_NONE, True)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        self._invoke(*args)

    def _invoke(self, nodeptr):
        _ptr = gdb.parse_and_eval(nodeptr)
        node = dom_node_at(_ptr)
        dom_walk_tree(node)

class DOMDocumentShowCommand(gdb.Command):
    """Show a document at a given address."""

    def __init__(self):
        gdb.Command.__init__(self, "dom document show", gdb.COMMAND_DATA,
                             gdb.COMPLETE_NONE, True)

    def invoke(self, arg, from_tty):
        args = gdb.string_to_argv(arg)
        self._invoke(*args)

    def _invoke(self, docptr):
        _ptr = gdb.parse_and_eval(docptr)
        doc = dom_document_at(_ptr)
        dom_document_show(doc)

DOMCommand()

DOMNodeCommand()
DOMNodeShowCommand()
DOMNodeWalkCommand()

DOMDocumentCommand()
DOMDocumentShowCommand()
