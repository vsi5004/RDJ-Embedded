#!/usr/bin/env python3
"""Convert Unity test output to JUnit XML.

Usage:
    ./test_suite 2>&1 | python3 unity_to_junit.py <output.xml> [suite_name]
  or
    python3 unity_to_junit.py <output.xml> [suite_name] < unity_output.txt

Unity output format (per test):
    <file>:<line>:<test_name>:PASS
    <file>:<line>:<test_name>:FAIL: <message>
    <file>:<line>:<test_name>:IGNORE[: <message>]

Summary line (last non-empty line before OK/FAIL):
    N Tests X Failures Y Ignored
"""

import re
import sys
import xml.etree.ElementTree as ET
from xml.dom import minidom

TEST_LINE  = re.compile(r'^(.+?):(\d+):(\w+):(PASS|FAIL|IGNORE)(?::\s*(.*))?$')
SUMMARY    = re.compile(r'^(\d+) Tests (\d+) Failures (\d+) Ignored')


def parse(lines):
    tests = []
    total = failures = ignored = 0

    for line in lines:
        line = line.rstrip()
        m = TEST_LINE.match(line)
        if m:
            tests.append({
                'file':    m.group(1),
                'line':    m.group(2),
                'name':    m.group(3),
                'result':  m.group(4),
                'message': m.group(5) or '',
            })
            continue
        m = SUMMARY.match(line)
        if m:
            total, failures, ignored = int(m.group(1)), int(m.group(2)), int(m.group(3))

    return tests, total, failures, ignored


def build_xml(tests, total, failures, ignored, suite_name):
    suite = ET.Element('testsuite', {
        'name':     suite_name,
        'tests':    str(total or len(tests)),
        'failures': str(failures),
        'skipped':  str(ignored),
        'errors':   '0',
    })

    for t in tests:
        case = ET.SubElement(suite, 'testcase', {
            'classname': suite_name,
            'name':      t['name'],
            'file':      t['file'],
            'line':      t['line'],
        })
        if t['result'] == 'FAIL':
            fail = ET.SubElement(case, 'failure', {'message': t['message']})
            fail.text = f"{t['file']}:{t['line']}: {t['message']}"
        elif t['result'] == 'IGNORE':
            ET.SubElement(case, 'skipped', {'message': t['message']})

    return suite


def prettify(element):
    raw = ET.tostring(element, encoding='unicode')
    return minidom.parseString(raw).toprettyxml(indent='  ')


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <output.xml> [suite_name]", file=sys.stderr)
        sys.exit(1)

    out_path   = sys.argv[1]
    suite_name = sys.argv[2] if len(sys.argv) > 2 else 'unity'

    lines = sys.stdin.readlines()
    tests, total, failures, ignored = parse(lines)
    suite = build_xml(tests, total, failures, ignored, suite_name)

    with open(out_path, 'w') as f:
        f.write(prettify(suite))


if __name__ == '__main__':
    main()
