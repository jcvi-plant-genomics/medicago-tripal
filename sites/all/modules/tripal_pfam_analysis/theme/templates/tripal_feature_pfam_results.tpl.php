<?php
$feature = $variables['node']->feature;

/* The results from an PfamScan analysis for this feature are avaialble to
 * this template in a array of the following foramt:
 *
 *     $results[$analysis_id]['iprterms']
 *     $results[$analysis_id]['pfamterms']
 *     $results[$analysis_id]['analysis']
 *     $results[$analysis_id]['format']
 *
 * Because there may be multiple PfamScan analyses for this feature, they
 * are separated in the array by the $analysis_id key.  The deeper array 
 * structure is as follows
 *
 *     An arrray containing all of the IPR terms mapped to this feature. Each
 *     IPR term is an array with 3 elements. The first element is the IPR
 *     accession, the second is the name and the third is the description
 *       $results[$analysis_id]['iprterms']
 *
 *     A string indicating the XML format from which the original results 
 *     were obtained. Valid values are XML4 or XML5
 *       $results[$analysis_id]['format']
 *
 *     An array of terms, where PFAM:XXXXXXXX idicates a PFAM accession that
 *     is used as a key for the array.  All PFAM terms for all matches are stored here.
 *       $results[$analysis_id]['pfamterms']['PFAM:XXXXXXX']['category']
 *       $results[$analysis_id]['pfamterms']['PFAM:XXXXXXX']['name']
 *
 *     An array of terms. The variable IPRXXXXXX indicates an IPR accession
 *     that is used as a key for the array.
 *       $results[$analysis_id]['iprterms']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['ipr_name']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['ipr_desc']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['ipr_type']
 *
 *
 *     Each term may have one or more matches.  The variable $j indicates
 *     an index variable for iterating through the matches.
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['match_id']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['match_name']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['match_desc']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['match_dbname']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['evalue']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['score']
 *
 *     An array of terms, where PFAM:XXXXXXXX idicates a PFAM accession that
 *     is used as a key for the array.  PFAM terms are stored a second time
 *     here to associate them with the proper IPR.
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['pfamterms']['PFAM:XXXXXXX']['category']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['pfamterms']['PFAM:XXXXXXX']['name']
 *
 *     Each match can have multiple start and stop locations. The variable $k
 *     indicates an index variable for iterating through the locations
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_start']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_end']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_score']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_status']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evalue']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_level']
 *       $results[$analysis_id]['iprterms']['IPRXXXXXX']['matches'][$j]['locations'][$k]['match_evidence']
 */


if (property_exists($feature, 'tripal_analysis_pfam')) {
  if (property_exists($feature->tripal_analysis_pfam->results, 'xml')) {
    
    // iterate through the results. They are organized by analysis id
    $results = $feature->tripal_analysis_pfam->results->xml;
    
    if(count($results) > 0){

      foreach($results as $analysis_id => $details){
        $analysis   = $details['analysis'];
        $pfamterms   = $details['pfamterms'];
        $format     = $details['format'];

        // ANALYSIS DETAILS
        $aname = $analysis->name;
        if (property_exists($analysis, 'nid')) {
          $aname = l($aname, 'node/' . $analysis->nid, array('attributes' => array('target' => '_blank')));
        }
        $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);
        print "
          Analysis Name: $aname
          <br>Date Performed: $date_performed
        ";

        // ALIGNMENT SUMMARY
        $headers = array(
          'PFAM Term', 
   	  'Genome Coordinates',
	  'Protein Coordinates',
	  'Domain Coordinates',
	  'domain_score',
	  'expected_domain',
	  'total_score',
	  'expected_whole'
        );
        
        $rows = array();
        foreach ($pfamterms as $pfam_id => $pfamterm) {
          // skip entries with no integrated PFAM

	  $genome_loc = $pfamterm['gstart'] . '-' . $pfamterm['gend'];
	  $protein_loc = $pfamterm['pstart'] . '-' . $pfamterm['pend'];
	  $domain_loc = $pfamterm['dstart'] . '-' . $pfamterm['dend'];
	  $domain_score = $pfamterm['domain_score'];
	  $expected_domain = $pfamterm['expected_domain'];
	  $total_score = $pfamterm['total_score'];
	  $expected_whole = $pfamterm['expected_whole'];
          
          $rows[] = array(
              l($pfam_id, "http://www.ebi.ac.uk/interpro/entry/$pfam_id", array('attributes' => array('target' => '_blank'))),
	      $genome_loc,
	      $protein_loc,
	      $domain_loc,
	      $domain_score,
	      $expected_domain,
	      $total_score,
	      $expected_whole
          );
        } // end foreach ($pfamterms as $pfam_id => $pfamterm) {

        if (count($rows) == 0) {
          $rows[] = array(
            array(
              'data' => 'No results',
              'colspan' => '8',
            ),
          );
        }
        $table = array(
          'header' => $headers,
          'rows' => $rows,
          'attributes' => array(),
          'sticky' => FALSE,
          'caption' => '',
          'colgroups' => array(),
          'empty' => '',
        );
        // once we have our table array structure defined, we call Drupal's theme_table()
        // function to generate the table.
        print theme_table($table);
        print "<br>";
      }
    }
  }
  // for backwards compatibility we want to ensure that if any results are stored
  // as HTML that they can still be displayed.  Although Tripal Pfam Analysis
  // v2.0 no longer supports importing of HTML results.
  if(property_exists($feature->tripal_analysis_pfam->results, 'html') and 
     $feature->tripal_analysis_pfam->results->html) {
    $resultsHTML = $feature->tripal_analysis_pfam->results->html;

    // ANALYSIS DETAILS
    $aname = $analysis_name;
    if (property_exists($analysis, 'nid')) {
      $aname = l($aname, 'node/' . $analysis->nid, array('attributes' => array('target' => '_blank')));
    }
    $date_performed = preg_replace("/^(\d+-\d+-\d+) .*/", "$1", $analysis->timeexecuted);
    print "
      Analysis Name: $analysis_name
      <br>Date Performed: $date_performed
    "; ?>
    
    <div class="tripal_feature-pfam_results_subtitle">Summary of Annotated IPR terms</div> <?php 
    print $resultsHTML;?>
    </div> <?php 
  }
}