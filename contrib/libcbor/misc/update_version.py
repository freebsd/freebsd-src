import sys, re
from datetime import date

version = sys.argv[1]
release_date = date.today().strftime('%Y-%m-%d')
major, minor, patch = version.split('.')


def replace(file_path, pattern, replacement):
    updated = re.sub(pattern, replacement, open(file_path).read())
    with open(file_path, 'w') as f:
        f.write(updated)

# Update changelog
SEP = '---------------------'
NEXT = f'Next\n{SEP}'
changelog_header = f'{NEXT}\n\n{version} ({release_date})\n{SEP}'
replace('CHANGELOG.md', NEXT, changelog_header)

# Update Doxyfile
DOXY_VERSION = 'PROJECT_NUMBER         = '
replace('Doxyfile', DOXY_VERSION + '.*', DOXY_VERSION + version)

# Update CMakeLists.txt
replace('CMakeLists.txt',
        '''SET\\(CBOR_VERSION_MAJOR "0"\\)
SET\\(CBOR_VERSION_MINOR "7"\\)
SET\\(CBOR_VERSION_PATCH "0"\\)''',
        f'''SET(CBOR_VERSION_MAJOR "{major}")
SET(CBOR_VERSION_MINOR "{minor}")
SET(CBOR_VERSION_PATCH "{patch}")''')

# Update Sphinx
replace('doc/source/conf.py',
        """version = '.*'
release = '.*'""",
        f"""version = '{major}.{minor}'
release = '{major}.{minor}.{patch}'""")
