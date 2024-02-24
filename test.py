import glob, subprocess, sys, re
modes = []

for o in ['-O0', '-O1', '-O2', '-O3', '-Os', '-Oz']:
  for i in ['', '-fno-inline-functions']:
    for d in ['', '-DNDEBUG']:
      for m in ['-sMALLOC=dlmalloc', '-sMALLOC=emmalloc']:
        for s in ['', '-msimd128']:
          for l in ['', '-flto']:
            modes += [[o, d, i, m, s, l]]

# Uncomment for quick testing in one mode.
#modes = [['-O3', '-g2', '-DNDEBUG', '-mbulk-memory', '-sMALLOC=emmalloc', '-flto', '-msimd128']]
modes = [['-O0', '-g2']]

tests = glob.glob('test/*.c')
if len(sys.argv) > 1:
  sub = sys.argv[1]
  tests = filter(lambda t: sub in t, tests)

cmd = ['emcc.bat', 'emgc.c', '-o', 'a.html', '-I.', '--js-library', 'test/library_test.js', '--js-library', 'lib_emgc.js']

failures = []
passes = 0

for m in modes:
  for t in tests:
    c = cmd + m + [t]
    run_in_browser = '// run: browser' in open(t, 'r').read()
#    if run_in_browser:
#      c += ['--emrun']
    flags = re.findall(r"// flags: (.*)", open(t, 'r').read())
    if len(flags) > 0:
      for f in flags:
        c += f.split(' ')
    print(' '.join(c))
    try:
      subprocess.check_call(c)
      if run_in_browser:
        subprocess.check_call(['emrun.bat', 'a.html'])
      else:
        subprocess.check_call(['node', 'a.js'])
      passes += 1
    except Exception as e:
      print(str(e))
      failures += [c]

for f in failures:
  cmd = ' '.join(f)
  print(f'FAIL: {cmd}')
print('')
print(f'{passes}/{passes+len(failures)} tests passed.')