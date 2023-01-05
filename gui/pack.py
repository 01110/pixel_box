#!/usr/bin/env python3

import os
import shutil
import gzip

dir_js = 'js'
dir_css = 'css'
dir_html = 'html'
dir_output = 'output'
dir_data = os.path.dirname(os.path.realpath(__file__))+'/../data'

def copy(source, target):
  files = os.listdir(source)
  for file_name in files:
    shutil.copy(source+'/'+file_name, target+'/'+file_name)

#create output directory
if os.path.exists(dir_output)==True:
  shutil.rmtree(dir_output)
os.mkdir(dir_output)

#copy css content & js content
copy(dir_css, dir_output)
copy(dir_js, dir_output)

#compress files
output_files = os.listdir(dir_output)
for file_name in output_files:
  with open(dir_output+'/'+file_name, 'rb') as f_in:
    with gzip.open(dir_output+'/'+file_name+'.gz', 'wb') as f_out:
      shutil.copyfileobj(f_in, f_out)
  os.remove(dir_output+'/'+file_name)

#copy html
copy(dir_html, dir_output)  

#copy output to data
if os.path.exists(dir_data)==True:
  shutil.rmtree(dir_data)
os.mkdir(dir_data)

copy(dir_output, dir_data)
