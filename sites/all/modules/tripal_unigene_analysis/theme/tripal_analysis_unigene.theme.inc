<?php 


/*******************************************************************************
 *  
 */
function tripal_analysis_unigene_preprocess_tripal_organism_unigenes(&$variables) {
   $node = $variables['node'];
   $organism = $node->organism;
   $unigenes = tripal_analysis_unigene_load_organism_unigenes($organism);
   $node->organism->tripal_analysis_unigene->unigenes =  $unigenes;
}

/*******************************************************************************
 *  
 */
function tripal_analysis_unigene_preprocess_tripal_feature_unigenes(&$variables) {
   $node = $variables['node'];
   $feature = $node->feature;
   $unigenes = tripal_analysis_unigene_load_feature_unigenes($feature);
   $node->feature->tripal_analysis_unigene->unigenes =  $unigenes;
}

/*******************************************************************************
 * tripal_analysis_unigene_results ()
* Prepare unigene result for the feature shown on the page
*/
function theme_tripal_analysis_unigene_feature_alignments($node) {
  $feature = $node->feature;
  $obj_feature = tripal_analysis_unigene_get_alignments ( $feature );
  $alignments = $obj_feature->alignments;
  if (count ( $alignments ) > 0) {
    $content = "<div id=\"tripal_analysis_unigene_box\" class=\"tripal_unigene-info-box\">";
    // we're showing contig alignments in GBrowse so create a link here for
    // that if this feature is a contig
    if ($node->feature->cvname == 'contig') {
      $content .= "<div class=\"tripal_expandableBox\">" . "  <h3>ESTs in this contig</h3>" . "</div>";
      $content .= "<div class=\"tripal_expandableBoxContent\">";
    } else {
      $content .= "<div class=\"tripal_expandableBox\">" . "  <h3>Alignments</h3>" . "</div>";
      $content .= "<div class=\"tripal_expandableBoxContent\">";
    }
    $content .= "" . "<table class=\"tripal_table_horz\">" . "  <tr>" . "    <th>Type</th>" . "    <th>Feature</th>" . "    <th align=\"right\">Position</th>" . "  </tr>";
    // iterate through each alignment
    foreach ( $alignments as $result ) {
      // EST alignments in chado use an EST_match type to map ESTs to
      // contigs and a rank to indicate the major srcfeature.
      // We don't want to show EST_matches on the alignment view
      // since that doesn't make much sense to the end user. If this
      // is an EST_match and the feature is an EST then we want to show
      // the contig in the alignments. The contig name is part of the
      // uniquename in the EST_match
      if ($node->feature->cvname == 'EST' && $result->cvname == 'EST_match') {
        $sql = "SELECT srcfeature_id " . "FROM {featureloc} " . "WHERE feature_id = :feature_id " . "ORDER BY rank";
        $contig_fid = chado_query ( $sql, array (
            ':feature_id' => $result->feature_id 
        ) )->fetchField ();
        $sql = "SELECT name FROM {feature} WHERE feature_id = :feature_id";
        $contig_name = chado_query ( $sql, array (
            ':feature_id' => $contig_fid 
        ) )->fetchField ();
        $sql = "SELECT nid " . "FROM {chado_feature} " . "WHERE feature_id = :feature_id";
        $contig_nid = db_query ( $sql, array (
            ':feature_id' => $contig_fid 
        ) )->fetchField ();
        // Check if the EST exists as a drupal node. If yes, add a link to
        // it. If no, just show the name
        if ($contig_nid != 0) {
          $sql = "SELECT nid FROM {node} WHERE nid=:nid";
          $node_exists = db_query ( $sql, array (
              ':nid' => $contig_nid 
          ) )->fetchField ();
        }
        $content .= "<tr>" . "  <td>Contig</td>" . "  <td>";
        if ($node_exists != 0) {
          $content .= "<a href=\"" . url ( "node/$contig_nid" ) . "\">" . "$contig_name</a>";
        } else {
          $content .= $contig_name;
        }
        $content .= "  </td>" . "  <td align=\"right\">" . number_format ( $result->fmin ) . "-" . number_format ( $result->fmax ) . "  </td>" . "</tr>";
      } 

      elseif ($node->feature->cvname == 'contig' && $result->cvname == 'EST_match') {
        $sql = "SELECT vid " . "FROM {node} " . "WHERE title = :title " . "ORDER BY vid DESC";
        // since the feature name is also the node title we can look it up
        $est_node = db_query ( $sql, array (
            ':title' => $result->feature_name 
        ) )->fetchObject ();
        $content .= "<tr>" . "  <td>EST</td>" . "  <td>";
        // Check if the EST exists as a drupal node before adding a hyperlink
        if ($est_node->vid != 0) {
          $content .= "<a href=\"" . url ( "node/$est_node->vid" ) . "\">" . $result->feature_name . "</a>";
        } else {
          $content .= $result->feature_name;
        }
        $content .= "  </td>" . "  <td align=\"right\">" . number_format ( $result->fmin ) . "-" . number_format ( $result->fmax ) . "  </td>" . "</tr>";
      } else {
        $content .= "" . "<tr>" . "  <td>$result->cvname</td>" . "  <td>$result->feature_name</td>" . "  <td align=\"right\">$result->fmin</td>" . "  <td align=\"right\">$result->fmax</td>" . "  <td align=\"right\">$result->strand</td>" . "</tr>";
      }
    }
    $content .= "</table>";
    
    /*
     * if this is a contig then get the alignment if ($node->feature->cvname == 'contig') { // get the directory prefix $prefix = preg_replace("/^(\d*)\d{3}$/","$1", $node->feature_id); if (!$prefix) { $prefix = '0'; } $data_url = variable_get('chado_feature_data_url', 'sites/default/files/data'); $fh = fopen("$data_url/misc/$prefix/$node->feature->feature_id/alignment.txt", 'r'); if ($fh) { $content .= "<b>Alignment:</b><div class=\"tripal_feature_assembly_alignment\"><pre>"; while (!feof($fh)) { $content .= fgets($fh); } $content .="</pre></div>"; } fclose($fh); }
     */
    $content .= "</div></div>";
  }
  return $content;
}
