#!/usr/bin/env python
import json
import unittest
import ucl
import sys

if sys.version_info[:2] == (2, 7):
    unittest.TestCase.assertRaisesRegex = unittest.TestCase.assertRaisesRegexp


class TestUcl(unittest.TestCase):
    def test_no_args(self):
        with self.assertRaises(TypeError):
            ucl.load()

    def test_multi_args(self):
        with self.assertRaises(TypeError):
            ucl.load(0,0)

    def test_none(self):
        r = ucl.load(None)
        self.assertEqual(r, None)

    def test_int(self):
        r = ucl.load("a : 1")
        self.assertEqual(ucl.load("a : 1"), { "a" : 1 } )

    def test_braced_int(self):
        self.assertEqual(ucl.load("{a : 1}"), { "a" : 1 } )

    def test_nested_int(self):
        self.assertEqual(ucl.load("a : { b : 1 }"), { "a" : { "b" : 1 } })

    def test_str(self):
        self.assertEqual(ucl.load("a : b"), {"a" : "b"})

    def test_float(self):
        self.assertEqual(ucl.load("a : 1.1"), {"a" : 1.1})

    def test_empty_ucl(self):
        r = ucl.load("{}")
        self.assertEqual(r, {})

    def test_single_brace(self):
        self.assertEqual(ucl.load("{"), {})

    def test_single_back_brace(self):
        ucl.load("}")

    def test_single_square_forward(self):
        self.assertEqual(ucl.load("["), [])

    def test_invalid_ucl(self):
        with self.assertRaisesRegex(ValueError, "unfinished key$"):
            ucl.load('{ "var"')

    def test_comment_ignored(self):
        self.assertEqual(ucl.load("{/*1*/}"), {})

    def test_1_in(self):
        with open("../tests/basic/1.in", "r") as in1:
            self.assertEqual(ucl.load(in1.read()), {'key1': 'value'})

    def test_every_type(self):
        totest="""{
            "key1": value;
            "key2": value2;
            "key3": "value;"
            "key4": 1.0,
            "key5": -0xdeadbeef
            "key6": 0xdeadbeef.1
            "key7": 0xreadbeef
            "key8": -1e-10,
            "key9": 1
            "key10": true
            "key11": no
            "key12": yes
        }"""
        correct = {
                'key1': 'value',
                'key2': 'value2',
                'key3': 'value;',
                'key4': 1.0,
                'key5': -3735928559,
                'key6': '0xdeadbeef.1',
                'key7': '0xreadbeef',
                'key8': -1e-10,
                'key9': 1,
                'key10': 'true',
                'key11': 'false',
                'key12': 'true',
                }
        self.assertEqual(ucl.load(totest), correct)

    def test_validation_useless(self):
        with self.assertRaises(NotImplementedError):
            ucl.validate("","")

if __name__ == '__main__':
    unittest.main()
