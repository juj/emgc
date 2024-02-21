import glob, subprocess, sys
modes = []

for o in ['-O0', '-O1', '-O2', '-O3', '-Os', '-Oz']:
  for i in ['', '-fno-inline-functions']:
    for m in ['-sMALLOC=dlmalloc', '-sMALLOC=emmalloc']:
      for m in ['', '-msimd128']:
        modes += [[o, i, m]]

# Uncomment for quick testing in -O0 suite.
modes = [['-O3']]

tests = glob.glob('test/*.c')
if len(sys.argv) > 1:
  sub = sys.argv[1]
  tests = filter(lambda t: sub in t, tests)

cmd = ['emcc.bat', 'emgc.c', 'emgc-roots.c', '-o', 'a.js', '-I.', '--js-library', 'test/library_test.js', '-sBINARYEN_EXTRA_PASSES=--spill-pointers', '-sALLOW_MEMORY_GROWTH', '-sMAXIMUM_MEMORY=4GB']

failures = []
passes = 0

for m in modes:
  for t in tests:
    c = cmd + m + [t]
    print(' '.join(c))
    try: 
      subprocess.check_call(c)
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