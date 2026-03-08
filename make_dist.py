''' make_dist.py creates a distributable version of the project files.

Run this script, and then include the generated code under dist/ to your project.
'''
import shutil, sys, os, re

include_pattern = re.compile(r'^\s*#include\s*[<"]([^">]+)[">]')

def find_include_file(name, current_dir, include_paths):
  # First try relative to current file
  path = os.path.join(current_dir, name)
  if os.path.isfile(path):
    return path

  # Then try include paths
  for p in include_paths:
    path = os.path.join(p, name)
    if os.path.isfile(path):
      return path

  return None


def expand_file(filename, include_paths, included_files):
  filename = os.path.abspath(filename)

  if filename in included_files:
    return ""  # prevent recursive inclusion

  included_files.add(filename)

  output = []
  current_dir = os.path.dirname(filename)

  with open(filename, "r", encoding="utf-8") as f:
    for line in f:
      m = include_pattern.match(line)
      if m:
        inc_name = m.group(1)
        if inc_name not in ['emgc.h']:
          inc_file = find_include_file(inc_name, current_dir, include_paths)
        else:
          inc_file = None

        if inc_file:
          output.append(expand_file(inc_file, include_paths, included_files))
        else:
          output.append(line)  # keep include if not found
      else:
        output.append(line)

  return "".join(output)


def main():
  result = expand_file('src/emgc.c', ['src'], set())
  result = result.replace('#pragma once\n', '')
  os.makedirs('dist', exist_ok=True)
  with open('dist/emgc-amalgamation.c', "w", encoding="utf-8") as f:
    f.write(result)
  shutil.copyfile('src/emgc.h', 'dist/emgc.h')
  shutil.copyfile('src/libemgc.js', 'dist/libemgc.js')

if __name__ == "__main__":
    main()
