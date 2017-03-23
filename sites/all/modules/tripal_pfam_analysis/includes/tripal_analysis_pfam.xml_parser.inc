<?php

/**
 * @file
 * The pfam XML parser
 */
function tripal_analysis_pfam_parseXMLFile($analysis_id, $pfamxmlfile,
  $parsepfam, $query_re, $query_type, $query_uniquename, $job_id) {
  
  
  $transaction = db_transaction();
  print "\nNOTE: Loading of this PfamScan XML file is performed using a database transaction. \n" .
      "If the load fails or is terminated prematurely then the entire set of \n" .
      "insertions/updates is rolled back and will not be found in the database\n\n";
  try {
  
    // clear out the anslysisfeature table for this analysis before getting started
    //tripal_core_chado_delete('analysisfeature', array('analysis_id' => $analysis_id));
  
    // If user input a file(e.g. pfam.xml)
    if (is_file($pfamxmlfile)) {
      tripal_analysis_pfam_parseSingleXMLFile($analysis_id, $pfamxmlfile,
          $parsepfam, $query_re, $query_type, $query_uniquename, $job_id, 1);
    }
    else {
      $dir_handle = @opendir($pfamxmlfile) or die("Unable to open $pfamxmlfile");
      $pattern = sql_regcase($pfamxmlfile . "/*.xml");
      $total_files = count(glob($pattern));
      print "$total_files file(s) to be parsed.\n";
  
      $interval = intval($total_files * 0.01);
      if ($interval == 0) {
        $interval = 1;
      }
      $no_file = 0;
  
      // Parsing all files in the directory
      while ($file = readdir($dir_handle)) {
        if (preg_match("/^.*\.xml/i", $file)) {
          tripal_analysis_pfam_parseSingleXMLFile($analysis_id, "$pfamxmlfile/$file",
            $parsepfam, $query_re, $query_type, $query_uniquename, $job_id, 0, $no_file, $total_files);        
        }
        $no_file ++;
      }
    }
  }
  catch (Exception $e) {
    print "\n"; // make sure we start errors on new line
    watchdog_exception('tr_ipr_parse', $e);
    $transaction->rollback();
    print "FAILED: Rolling back database changes...\n";
  }
  print "\nDone\n";
}
/**
 *
 */
function tripal_analysis_pfam_parseSingleXMLFile($analysis_id, $pfamxmlfile,
  $parsepfam, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status = 1, 
  $file_no = 0, $total_files = 1) {
      
  $filename = preg_replace('/^.*\/(.*)$/' , '$1', $pfamxmlfile);

  // Load the XML file
  $xml =  simplexml_load_file($pfamxmlfile);
  if (!$xml) {
    watchdog('tripal_pfam', "Cannot open XML. Please check that it is valid XML.", NULL, WATCHDOG_ERROR);
    return;
  }

  // If starts with 'pfam-matches' or 'nucleotide-sequence-matches' then this is a custom v4 XML
  if (preg_match('/^pfam_matches/', $xml->getname())) {
    // rseults are from the command-line InterPrScan version
    tripal_analysis_pfam_parseXML4($xml, $analysis_id, $filename,
      $parsepfam, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files);
  }
  else {
    watchdog('tr_ipr_parse', "ERROR: cannot parse XML file format is not recognized", array(), WATCHDOG_ERROR);
    return;
  }
}


/**
 * 
 */
function tripal_analysis_pfam_add_analysis_feature($feature_id, $analysis_id, $xml_obj) {
  
  // Get cvterm_id for 'analysis_pfam_xmloutput_hits' which is required
  // for inserting into the analysisfeatureprop table
  $sql = "
    SELECT
      CVT.cvterm_id
    FROM {cvterm} CVT
      INNER JOIN {cv} ON cv.cv_id = CVT.cv_id
    WHERE
      CVT.name = 'analysis_pfam_xmloutput_hit' AND
      CV.name = 'tripal'
  ";
  $type_id = chado_query($sql)->fetchField();
  
  // Insert into analysisfeature table only if it doesn't already exist
  $values = array('feature_id' => $feature_id, 'analysis_id' => $analysis_id);
  $analysisfeature = tripal_core_chado_select('analysisfeature', array('*'), $values);
  if (count($analysisfeature) == 0) {
    $analysisfeature = tripal_core_chado_insert('analysisfeature', $values);
    $analysisfeature_id = $analysisfeature['analysisfeature_id'];
  }
  else {
    $analysisfeature_id = $analysisfeature[0]->analysisfeature_id;
  }
  
  // Insert pfam xml results into analysisfeatureprop table
  // but first check to see if we have an existing entry, if we do, then
  // increment the rank
  $sql = "
    SELECT analysisfeatureprop_id, rank
    FROM {analysisfeatureprop}
    WHERE analysisfeature_id = :analysisfeature_id AND type_id = :type_id
    ORDER BY rank DESC
  ";
  $args = array(':analysisfeature_id' => $analysisfeature_id, ':type_id' => $type_id); 
  $result = chado_query($sql, $args)->fetchObject();
  $rank = 0;
  if ($result) {
    $rank = $result->rank + 1;
  }
  
  $values = array(
    'analysisfeature_id' => $analysisfeature_id,
    'type_id' => $type_id,
    'value' => $xml_obj->asXML(),
    'rank' => $rank,
  );
  $success = tripal_core_chado_insert('analysisfeatureprop', $values);
  if (!$success) {
    watchdog('tr_ipr_parse', 'ERROR: cannot add the XML for the feature into the analysisfeatureprop table',
    array(), WATCHDOG_ERROR);
  }
  
  return $analysisfeature_id;
}
/**
 * @param $xml
 */
function tripal_analysis_pfam_parseXML4($xml, $analysis_id, $filename,
      $parsepfam, $query_re, $query_type, $query_uniquename, $job_id, $uptate_status, 
      $file_no, $total_files) {

  if (preg_match('/^pfam_matches/', $xml->getname())) {
      $proteins = $xml;
  }

  // Count the number of entries to be processed
  $no_iterations = 0;
  foreach ($proteins as $protein) {
    $no_iterations++;
  }
  $interval = intval($no_iterations * 0.01);
  if ($interval == 0) {
    $interval = 1;
  }
  $idx_iterations = 0;

  // Processed each protein
  foreach ($proteins as $protein) {
    
    // Set job status
    $idx_iterations ++;    
    if ($idx_iterations % $interval == 0) {
      $percentage = sprintf("%.4f",  (($file_no + ($idx_iterations / $no_iterations)) / $total_files) * 100);
      tripal_job_set_progress($job_id, $percentage);
      print "Parsing file, $filename, $file_no of $total_files  ($percentage%). Memory: " . number_format(memory_get_usage()) . " bytes\r";
    }

    // match the protein id with the feature name
    $feature_id = 0;
    $attrs = $protein->attributes();
    $seqname = $attrs['id'];

    // is the sequence name a generic name (i.e. 'Sequence_1') then the
    // results do not contain the original sequence names.  The only
    // option we have is to use the filename.  This will work in the case of
    // Blast2PFAM which stores the XML for each sequence in a file with the
    // the filename the name of the sequence
    if (preg_match('/Sequence_\d+/', $seqname)) {
      $filename = preg_replace('/^.*\/(.*).xml$/', '$1', $pfamxmlfile);
      watchdog('tr_ipr_parse', "Sequence name for results is not specific, using filename: %filename as the sequence name\n", array('%filename' => $filename), WATCHDOG_NOTICE);
      $seqname = $filename;
    }

    // Remove _ORF from the sequence name
    $seqname = preg_replace('/^(.+)_\d+_ORF\d+.*/', '$1', $seqname);
    
    // match the name of the feature in the XML file to a feature in Chado
    $feature_id = tripal_analysis_pfam_match_feature($seqname, $query_re, $query_uniquename);
    if (!$feature_id) {
      continue;
    }

    //msarmien: adds the XML protein tag component to the analysisfeatureprop table
    // create an entry in the analysisfeature table and add the XML for this feature
    // to the analysisfeatureprop table
    $analysisfeature_id = tripal_analysis_pfam_add_analysis_feature($feature_id, $analysis_id, $protein);
    if (!$analysisfeature_id) {
      continue;
    }
     
    // parse the XML
    $ipr_array = tripal_analysis_pfam_parse_feature_XML($protein->asXML(), $feature_id);
    
    // get the DB id for the PFAM database
    $parsepfam = tripal_analysis_get_property($analysis_id, 'analysis_pfam_parsepfam');
    $pfam_db_id = chado_query("SELECT db_id FROM {db} WHERE name='PFAM'")->fetchField();
    if ($parsepfam and !$pfam_db_id) {
      watchdog('tr_ipr_parse', 'PFAM schema not installed in chado. PFAM terms are not processed.', array(), WATCHDOG_WARNING);
    }

    // add in the PFAM Terms
    if ($parsepfam and $pfam_db_id) {
      $pfamterms = $ipr_array['pfamterms'];
      tripal_analysis_pfam_load_pfamterms($pfamterms, $feature_id, $analysisfeature_id, $pfam_db_id);
    }

  } // end foreach ($proteins as $protein) 
     
  return;
}

/**
 * Attempts to find a feature in Chado that matches the sequence id as listed
 * in the PfamScan XML file
 * 
 * @param $seqid
 *   The sequence name provided by the PfamXML file
 * @param $query_re
 *   The regular expression provided by the user when the chado_analysis_pfam
 *   node was created by the user
 * @param $query_uniquename
 *   The value provided by the user when the chado_analysis_pfam node was
 *   created that indicates TRUE if the $seqid is the uniquename. Otherwise
 *   it is assumed to just be the feature name.
 *   
 * @return
 *   The feature_id of the matching feature or NULL if not found.  
 */
function tripal_analysis_pfam_match_feature($seqid, $query_re, $query_uniquename) {

  $feature = '';
  
  // if a regular expression is provided then pick out the portion requested
  $matches = array();
  if ($query_re and preg_match("/$query_re/", $seqid, $matches)) {
    $feature = $matches[1];
  }
  // If no match by the regular expression then get everything up to the first space
  else {
    if (preg_match('/^(.*?)\s.*$/',  $seqid, $matches)) {
      $feature = $matches[1];
    }
    // if no match up to the first space then just use the entire string
    else {
      $feature =  $seqid;
    }
  }
  
  if (!$feature and $query_re) {
    watchdog('tr_ipr_parse', "Failed: Cannot find feature for '%seqname' using the regular expression: %query_re",
      array('%seqname' => $seqname, '%query_re' => $query_re));
    return NULL;
  }
  
  // now find the feature in chado
  $select = array();
  if ($query_uniquename) {
    $select['uniquename'] = $feature;
  }
  else {
    $select['name'] = $feature;
  }
  if ($query_type) {
    $select['type_id'] = array(
      'cv_id' => array(
        'name' => 'sequence'
      ),
      'name' => $query_type,
    );
  }
  
  $feature_arr = tripal_core_chado_select('feature', array('feature_id'), $select);
  if (count($feature_arr) > 1) {
    watchdog('tr_ipr_parse', "Ambiguous: '%feature' matches more than one feature and is being skipped.",
    array('%feature' => $feature), WATCHDOG_WARNING);
    return NULL;
  }
  if (count($feature_arr) == 0) {
    watchdog('tr_ipr_parse', "Failed: cannot find a matching feature for '%feature' in the database.",
    array('%feature' => $feature), WATCHDOG_ERROR);
    return NULL;
  }
  $feature_id = $feature_arr[0]->feature_id;
  
  return $feature_id;
}

/**
 * Adds PFAM terms to the analysisfeatureprop  table and the feature_cvterm
 * table for the feature.
 *  
 * @param $pfamterms
 *   An array containin each PFAM terms in the following format:
 *     $pfamterms['PFAM:XXXXXXX']['catepfamry']
 *     $pfamterms['PFAM:XXXXXXX']['name']
 * @param $feature_id
 *   The feature to which the terms should be assocaited
 * @param $analysisfeature_id
 *   The analysisfeature to which the terms should be listed as properties 
 * @param $pfam_db_id
 *   The db_id for the PFAM (Gene Ontology)
 */
function tripal_analysis_pfam_load_pfamterms($pfamterms, $feature_id, $analysisfeature_id, $pfam_db_id) {
  
  // cycle through the PFAM terms and add them to the database
  foreach ($pfamterms as $pfam_id => $pfamterm) {
     
    // seperate the 'PFAM:' from the term
    if (preg_match("/^.*?PFAM:(\d+).*$/", $pfam_id, $matches)) {
  
      // Find cvterm_id for the matched PFAM term
      $sql = "
        SELECT cvterm_id
        FROM {cvterm} CVT
          INNER JOIN {dbxref} DBX ON CVT.dbxref_id = DBX.dbxref_id
        WHERE DBX.accession = :accession AND DBX.db_id = :db_id
      ";

      $args = array(':accession' => $matches[1], ':db_id' => $pfam_db_id);
      $pfamterm_id = chado_query($sql, $args)->fetchField();
      if (!$pfamterm_id) {
        watchdog('tr_ipr_parse', "Cannot find PFAM cvterm: 'PFAM:%term'. skipping.",
          array('%term' => $matches[1]), WATCHDOG_WARNING);
        continue;
      }
  
      // Insert PFAM terms into feature_cvterm table. Default pub_id = 1 (NULL) was used. But
      // only insert if not already there
      $values = array(
        'feature_id' => $feature_id,
        'cvterm_id' => $pfamterm_id,
        'pub_id' => 1
      );
      $feature_cvterm = tripal_core_chado_select('feature_cvterm', array('*'), $values);
      if (sizeof($feature_cvterm) == 0) {
        $feature_cvterm = tripal_core_chado_insert('feature_cvterm', $values);
        if (!$feature_cvterm) {
          watchdog('tr_ipr_parse', "ERROR:  Cannot add cvterm to the feature for term: %pfamterm_id'\n",
          array('%pfamterm_id' => $pfamterm_id), WATCHDOG_ERROR);
          continue;
        }
      }
  
      // Insert PFAM terms into analysisfeatureprop table
      $values = array(
        'analysisfeature_id' => $analysisfeature_id,
        'type_id' => $pfamterm_id,
        'rank' => 0
      );
      $analysisfeatureprop = tripal_core_chado_select('analysisfeatureprop', array('*'), $values);
      if (sizeof($analysisfeatureprop) == 0) {
        $values['value'] = $matches[1];
        $analysisfeatureprop = tripal_core_chado_insert('analysisfeatureprop', $values);
      }
    } // end if preg_match
  } // end for each pfamterm
}
/**
 * Parses the Pfam XML for a single feature and returns an array
 * containing the results.
 *
 * @param $pfam_xml
 *   The XML results for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *
 * @return
 *   An array with the following fields:
 *
 *     An arrray containing all of the IPR terms mapped to this feature. Each
 *     IPR term is an array with 3 elements. The first element is the IPR
 *     accession, the second is the name and the third is the description
 *       $terms['iprterms']
 *
 *     A string indicating the XML format that was parsed. Valid values
 *     are XML4 or XML5
 *       $terms['format']
 *
 *     An array of terms, where PFAM:XXXXXXXX idicates a PFAM accession that
 *     is used as a key for the array.  All PFAM terms for all matches are stored here.
 *       $terms['pfamterms']['PFAM:XXXXXXX']['catepfamry']
 *       $terms['pfamterms']['PFAM:XXXXXXX']['name']
 *
 *     An array of terms. The variable IPRXXXXXX indicates an IPR accession
 *     that is used as a key for the array.
 *       $terms['iprterms']
 *       $terms['iprterms']['IPRXXXXXX']['ipr_name']
 *       $terms['iprterms']['IPRXXXXXX']['ipr_desc']
 *       $terms['iprterms']['IPRXXXXXX']['ipr_type']
 *
 *     Each term may have one or more matches.  The variable $j indicates
 *     an index variable for iterating through the matches.
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_id']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_name']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_desc']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['match_dbname']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['evalue']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['score']
 *
 *     An array of terms, where PFAM:XXXXXXXX idicates a PFAM accession that
 *     is used as a key for the array.  PFAM terms are stored a second time
 *     here to associate them with the proper IPR.
 *       $terms['iprterms']['IPRXXXXXX']['pfamterms']['PFAM:XXXXXXX']['catepfamry']
 *       $terms['iprterms']['IPRXXXXXX']['pfamterms']['PFAM:XXXXXXX']['name']
 *
 *     Each match can have multiple start and stop locations. The variable $k
 *     indicates an index variable for iterating through the locations
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_start']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_end']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_score']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_status']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evalue']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_level']
 *       $terms['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evidence']
 */
function tripal_analysis_pfam_parse_feature_XML($pfam_xml, $feature_id) {

  // Load the XML into an object
  $xml_obj = simplexml_load_string($pfam_xml);
  
  // we can distinguish between XML4 and XML5 by looking at the name and attributes
  $name  = $xml_obj->getname();
  $attrs = $xml_obj->attributes();
  
  if ($name == 'protein') {
    return tripal_analysis_pfam_parse_feature_XML4($xml_obj, $feature_id);
  }
}


/**
 * Parses the Pfam XML for a single feature and returns an array
 * containing the results.
 *
 * @param $xml_obj
 *   The SimpleXMLElement object for a single feature.
 * @param $feature_id
 *   The feature ID to which this blast XML belongs.
 *
 * @return
 *  An array as specified by the tripal_analysis_pfam_parse_feature_XML()
 *  function
 */
function tripal_analysis_pfam_parse_feature_XML4($xml_obj, $feature_id) {

  $terms  = array();
  $terms['format'] = 'XML4';
  $terms['pfamterms'] = array();

  // get the properties of this result
  $attr = $xml_obj->attributes();
  $protein['orf_id']     = (string) $attr["id"];

  // iterate through each pfam results for this protein
    foreach ($xml_obj->children() as $level1) {
      $element_name = $level1->getName();

      // get the PFAM terms for this match
      if ($element_name == 'classification') {
        $attrs = $level1->attributes();
        if ($attrs['class_type'] == 'PFAM') {
          $pfam_id = (string) $attrs['id'];

	   foreach ($level1->children() as $level2) {
      	   	   $element_name2 = $level2->getName();

		   if($element_name2 == 'hit'){
		     $attrs2 = $level2->attributes();

          	     $pfamterm = array();
          	     $pfamterm['gstart'] = $attrs2['gstart'];
          	     $pfamterm['gend']   = $attrs2['gend'];
          	     $pfamterm['pstart'] = $attrs2['pstart'];
          	     $pfamterm['pend']   = $attrs2['pend'];
          	     $pfamterm['dstart'] = $attrs2['dstart'];
          	     $pfamterm['dend']   = $attrs2['dend'];
          	     $pfamterm['domain_score'] = $attrs2['domain_score'];
          	     $pfamterm['expected_domain']   = $attrs2['expected_domain'];
          	     $pfamterm['total_score'] = $attrs2['total_score'];
          	     $pfamterm['expected_whole']   = $attrs2['expected_whole'];
		   }
	   }
          // PFAM terms are stored as first-level element of the $terms array where all terms are present
          $terms['pfamterms'][$pfam_id] = $pfamterm;
        }
      }    
  }
  
  return $terms;
}
