import os
import re
import sys
import tempfile
import subprocess

def _run_flex(filename):
  fd, path = tempfile.mkstemp()
  os.close(fd)
  flex = subprocess.Popen(
    ['flex', '-B', '--8bit', '--noline', '-o', path, filename],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
  )
  stdout, stderr = flex.communicate()
  sys.stdout.write(stdout.decode("utf8"))
  sys.stderr.write(stderr.decode("utf8"))
  if flex.returncode:
    raise Exception('flex failed to run')
  output = open(path, "r").read()
  # print("output:", output)
  os.remove(path)
  return output

def _find_array(source, name):
  match = re.search(name + r'\[\d+\]\s*=[^{]*\{([^}]*)\}', source)
  return map(int, match.groups(1)[0].split(','))

def _find_magic_number(source, pattern):
  match = re.search(pattern, source)
  return int(match.groups(1)[0])

def _find_actions(source):
  matches = re.findall(r'case\s+(\d+)\s*:\s*(?:/\*[^*]*\*/)?\s*YY_RULE_SETUP[^\0]*?\n(.*);\s*YY_BREAK', source)
  return [(int(m[0]), m[1]) for m in matches]

def flex_compile(filename):
  output = _run_flex(filename)
  result = {}

  # Comments from https://github.com/gobo-eiffel/gobo/blob/master/library/lexical/scanner/lx_compressed_tables.e
  result['yy_accept'] = _find_array(output, 'yy_accept') # Accepting id list
  result['yy_ec']     = _find_array(output, 'yy_ec') # ASCII to equivalence class
  result['yy_meta']   = _find_array(output, 'yy_meta') # Meta equivalence classes which are sets of classes with identical transitions out of templates
  result['yy_base']   = _find_array(output, 'yy_base') # Offsets into 'yy_nxt' for given states
  result['yy_def']    = _find_array(output, 'yy_def') # Where to go if 'yy_chk' disallows 'yy_nxt' entry
  result['yy_nxt']    = _find_array(output, 'yy_nxt') # States to enter upon reading symbol
  result['yy_chk']    = _find_array(output, 'yy_chk') # Check value to see if 'yy_nxt' applies

  result['yy_end_of_buffer'] = _find_magic_number(output, r'#define\s+YY_END_OF_BUFFER\s+(\d+)')
  result['jamstate'] = _find_magic_number(output, r'while\s*\(\s*yy_current_state\s*!=\s*(\d+)')
  result['actions'] = _find_actions(output)

  return result



template = '''
// Automatically generated by %(generator)s; Do not edit.

typedef enum Tok {
%(actions)s,
  T_MAX
} Tok;

%(yy_accept)s
%(yy_ec)s
%(yy_meta)s
%(yy_base)s
%(yy_def)s
%(yy_nxt)s
%(yy_chk)s
%(jamstate)s
%(yy_accept_length)s
'''

def create_table(result, name, type=None):
  entries = ['% 3s' % x for x in result[name]]

  entries2 = ""
  z = len(entries)-1
  linelen = 0
  for i in range(0, z+1):
    linelen_incr = len(entries[i]) + 2
    linelen += linelen_incr
    if i == 0 or linelen > 79:
      linelen = linelen_incr
      entries2 += "\n "
    entries2 += " " + entries[i]
    entries2 += "," if i < z else " "

  return 'const %(type)s %(name)s[] = {%(entries)s\n};' % {
    'type': type if type else "u8",
    'name': name,
    'entries': entries2,
  }

# Read and compile the input
path = os.path.dirname(__file__)
result = flex_compile(os.path.join(path, 'lex.l'))
# print(result)

# Assume all actions are sequential and start at 1
if [k for k, v in result['actions']] != list(range(1, len(result['actions']) + 1)):
  raise Exception('all actions are not sequential')

# Assume ECHO is the last action
if result['actions'][-1][1] != 'ECHO':
  raise Exception('ECHO is not after the last action')

# Assume yy_end_of_buffer is after the last action
if result['yy_end_of_buffer'] != len(result['actions']) + 1:
  raise Exception('yy_end_of_buffer is not after the last action')

# Patch the results
result['actions'] = dict(
  (k, v if v != 'ECHO' else 'ERROR')
  for k, v in result['actions'] + [
    (0, 'YY_INVALID_ACTION'),
    (result['yy_end_of_buffer'], 'END_OF_FILE')
  ]
)
result['yy_accept'] = ['T_%s' % result['actions'][x] for x in result['yy_accept']]
result['actions'] = ',\n'.join('  T_%s' % x for x in sorted(set(result['actions'].values())))
result['yy_accept_length'] = len(result['yy_accept'])
result['yy_accept'] = create_table(result, 'yy_accept', 'Tok')
result['yy_ec'] = create_table(result, 'yy_ec')
result['yy_meta'] = create_table(result, 'yy_meta')
result['yy_base'] = create_table(result, 'yy_base')
result['yy_def'] = create_table(result, 'yy_def')
result['yy_nxt'] = create_table(result, 'yy_nxt')
result['yy_chk'] = create_table(result, 'yy_chk')
result['jamstate'] = 'const YY_JAM_STATE = %s' % result['jamstate']
result['yy_accept_length'] = 'const YY_ACCEPT_LENGTH = %s' % result['yy_accept_length']
result['generator'] = __file__

# Write the output
open(os.path.join(path, 'lex.c'), 'w').write(template.strip() % result + '\n')
