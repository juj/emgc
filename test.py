#! /usr/bin/python3
import glob, subprocess, sys, re
modes = []

for o in ['-O0', '-O1', '-O2', '-O3', '-Os', '-Oz']:
  for i in ['', '-fno-inline-functions']:
    for d in ['', '-DNDEBUG']:
      for m in ['-sMALLOC=dlmalloc', '-sMALLOC=emmalloc']:
        for s in ['', '-msimd128']:
          for l in ['', '-flto']:
            modes += [[o, d, i, m, s, l]]

skip_browser_tests = True if '--skip-browser-tests' in sys.argv else False

argv = list(filter(lambda t: not t.startswith('--'), sys.argv[1:]))

# Uncomment for quick testing in one mode.
#modes = [['-O3', '-g2', '-DNDEBUG', '-mbulk-memory', '-sMALLOC=emmalloc', '-flto', '-msimd128']]
modes = [['-O3', '-g2']]

tests = glob.glob('test/*.c')
if len(argv) > 0:
  sub = argv[0]
  tests = filter(lambda t: sub in t, tests)

def bat_suffix(executable):
  if sys.platform == 'win32':
    return f'{executable}.bat'
  return executable

cmd = [bat_suffix('emcc'), 'emgc.c', '-o', 'a.html', '-I.', '--js-library', 'test/library_test.js', '--js-library', 'lib_emgc.js']

failures = []
passes = 0

for m in modes:
  for t in tests:
    c = cmd + m + [t]
    test_code = open(t, 'r').read()
    run_in_browser = '// run: browser' in test_code
    if run_in_browser and skip_browser_tests:
      print(f'--skip-browser-tests: Skipping browser test {c}')
      continue

#    if run_in_browser:
#      c += ['--emrun']
    flags = re.findall(r"// flags: (.*)", test_code)
    if len(flags) > 0:
      for f in flags:
        c += f.split(' ')
    print(' '.join(c))
    try:
      subprocess.check_call(c, shell=True)
      if run_in_browser:
        subprocess.check_call([bat_suffix('emrun'), 'a.html'], shell=True)
      else:
        subprocess.check_call(['node', 'a.js'], shell=True)
      passes += 1
    except Exception as e:
      print(str(e))
      failures += [c]

for f in failures:
  cmd = ' '.join(f)
  print(f'FAIL: {cmd}')
print('')
print(f'{passes}/{passes+len(failures)} tests passed.')
