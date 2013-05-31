#!/usr/bin/env python
# -*- coding: utf-8 -*-
# 
# Maintainer: A. Nagler
#
# Script for searching parameters which arem't used any longer

from read_ccarat_NIGHTLYTESTCASES import read_ccarat, write_ccarat
from elements    import bcdictionary, surfaces
from progress    import progress
from sets        import Set

import sys, subprocess

# Dictionary of unused parameters we want to keep in the code
UNUSED_MAT_TO_KEEP = []


if __name__=='__main__':
  
    if len(sys.argv) < 2:
      print "usage: %s source-path" % sys.argv[0]
      sys.exit(1)    
    
    # collect source files of src
    files_to_search = []
    global_src_path = sys.argv[1] + '/' + 'src/'
    
    source_headers = subprocess.check_output('ls --hide=*.a ' + global_src_path, shell=True)
    for sh in source_headers.split():
	baci_files = subprocess.check_output('ls ' + global_src_path + sh, shell=True)
	baci_files = baci_files.split()
	files_to_search.extend( [global_src_path + sh + '/' + (baci_files)[i] for i in range(len(baci_files)) ] )
    
    # valid parameters file will be neclected
    files_to_search.remove(global_src_path + 'drt_inpar/drt_validmaterials.cpp')
    
    print 'Start to search inpar materials'
    
    # Initialization of Set for unused inpar parameters. Set used to avoid double entries
    fail = Set()
		
    inpar_materials = subprocess.check_output('grep INPAR ' + global_src_path +  'drt_inpar/drt_validmaterials.cpp', shell=True )
    inpar_materials = inpar_materials.split()
    
    # partitioning necessary due to maximal size of input arguments of bash console
    files_to_search_part1 = files_to_search[:len(files_to_search)/2 + 1]
    files_to_search_part2 = files_to_search[len(files_to_search)/2 + 1:]
    
    # Grep for value in code. If the value doesn't appear than the input parameter might be unused
    # Check first part of files and if this failes check second part
    # If both fail than the argument doesn't exists  
	    
    for inpa_mat in progress('Searching inpar materials', inpar_materials):
	
	inpa_mat = inpa_mat.strip(' \ntuple<int>(),;')
	inpa_mat = (inpa_mat.split(','))[0]
	
	if inpa_mat[:5] == 'INPAR':
	  
	    try:
		if inpa_mat in UNUSED_MAT_TO_KEEP:
		    continue
	    except KeyError:
		pass
	      
	    try:
		test = subprocess.check_output( '/bin/grep ' +  inpa_mat + " " + " ".join(files_to_search_part1), shell=True)
	    except subprocess.CalledProcessError: 
		try:
		    test = subprocess.check_output( '/bin/grep ' +  inpa_mat + " " + " ".join(files_to_search_part2), shell=True)
		except subprocess.CalledProcessError: 
		    fail.update([inpa_mat])	    

    if not fail:
	print "Found no unused input material in code"		
    else:
	# Retype to list in order to be able to sort
	final_fail_printout = [f for f in fail]
	final_fail_printout.sort()
	print "The following input material only exist in drt_validmaterials.cpp"
	print "\n".join(final_fail_printout)
	sys.exit(1)	