import glob, subprocess
modes = []

for o in ['-O0', '-O1', '-O2', '-O3', '-Os', '-Oz']:
  for i in ['', '-fno-inline-functions']:
    for m in ['-sMALLOC=dlmalloc', '-sMALLOC=emmalloc']:
      modes += [[o, i, m]]

tests = glob.glob('test/*.cpp')

cmd = ['em++.bat', 'emgc.cpp', '-o', 'a.js', '-I.', '--js-library', 'test/library_test.js']

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
  print(f'FAIL: {str(f)}')
print('')
print(f'{passes}/{passes+len(failures)} tests passed.')