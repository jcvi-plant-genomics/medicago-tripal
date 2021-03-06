<?php

$node = $variables['node'];
$analysis = $variables['node']->analysis;

// the description is a text field so we want to expand that
$analysis = tripal_core_expand_chado_vars($analysis,'field','analysis.description');

// get the unigene data
$unigene = $node->analysis->tripal_analysis_unigene; ?>

<div class="tripal_analysis_unigene-info-box-desc tripal-info-box-desc"></div><?php 

// the $headers array is an array of fields to use as the colum headers. 
// additional documentation can be found here 
// https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
// This table for the library has a vertical header (down the first column)
// so we do not provide headers here, but specify them in the $rows array below.
$headers = array();

// the $rows array contains an array of rows where each row is an array
// of values for each column of the table in that row.  Additional documentation
// can be found here:
// https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7 
$rows = array();

// Analysis Name row
$rows[] = array(
  array(
    'data' => 'Analysis Name',
    'header' => TRUE,
    'width' => '20%',
  ),
  $analysis->name
);

// Unigene Name row
$rows[] = array(
    array(
        'data' => 'Unigene Name',
        'header' => TRUE,
        'width' => '20%',
    ),
    $unigene->unigene_name
);

// Implementation row
$software = $analysis->program;
if($analysis->programversion != 'n/a'){
  $software .=  " (" . $analysis->programversion . ")";
}
if($analysis->algorithm){
  $software .= ". " . $analysis->algorithm;
}
$rows[] = array(
  array(
    'data' => 'Method',
    'header' => TRUE
  ),
  $software
);

// Source row
$source = '';
if($analysis->sourceuri){
  $source = "<a href=\"$analysis->sourceuri\">$analysis->sourcename</a>";
}
else {
  $source = $analysis->sourcename;
}
if($analysis->sourceversion){
  $source = " (" . $analysis->sourceversion . ")";
}
$rows[] = array(
  array(
    'data' => 'Source',
    'header' => TRUE
  ),
  $source
);

// Date performed row
$rows[] = array(
  array(
    'data' => 'Date performed',
    'header' => TRUE
  ),
  preg_replace("/^(\d+-\d+-\d+) .*/","$1", $analysis->timeexecuted),
);

// Unigene Stats row
if ($unigene->num_reads or
     $unigene->num_clusters or
     $unigene->num_contigs or
     $unigene->num_singlets) {
  $stats = "";
  if ($unigene->num_reads) {
    $stats .= "Number of reads: $unigene->num_reads<br>";
  }
  if ($unigene->num_clusters) {
    $stats .= "Number of clusters: $unigene->num_clusters<br>";
  }
  if ($unigene->num_contigs) {
    $stats .= "Number of contigs: $unigene->num_contigs<br>";
  }
  if ($unigene->num_singlets) {
    $stats .= "Number of singlets: $unigene->num_singlets<br>";
  }  
  $rows [] = array (
      array (
          'data' => 'Stats',
          'header' => TRUE,
          'width' => '20%' 
      ),
      $stats
  );
}

// Unigene organism(s)
if (property_exists($unigene, 'organisms') and is_array ( $unigene->organisms )) {
  $orgs = "";
  foreach ( $unigene->organisms as $organism ) {
    if ($organism->nid) {
      $orgs .= "<i><a href=\"" . url ( "node/$organism->nid" ) . "\">$organism->genus $organism->species</i></a><br>";
    } else {
      $orgs .= "<i>$organism->genus $organism->species</i><br>";
    }
  }
  $rows [] = array (
      array (
          'data' => 'Organisms',
          'header' => TRUE,
          'width' => '20%'
      ),
      $orgs
  );
}

// allow site admins to see the analysis ID
if (user_access('administer tripal')) {
  // Analysis ID
  $rows[] = array(
    array(
      'data' => 'Analysis ID',
      'header' => TRUE,
      'class' => 'tripal-site-admin-only-table-row',
    ),
    array(
      'data' => $analysis->analysis_id,
      'class' => 'tripal-site-admin-only-table-row',
    ),
  );
}
// the $table array contains the headers and rows array as well as other
// options for controlling the display of the table.  Additional
// documentation can be found here:
// https://api.drupal.org/api/drupal/includes%21theme.inc/function/theme_table/7
$table = array(
  'header' => $headers,
  'rows' => $rows,
  'attributes' => array(
    'id' => 'tripal_analysis_unigene-table-base',
  ),
  'sticky' => FALSE,
  'caption' => '',
  'colgroups' => array(),
  'empty' => '',
);

// once we have our table array structure defined, we call Drupal's theme_table()
// function to generate the table.
print theme_table($table);
if (property_exists($analysis, 'description')) { ?>
<div style="text-align: justify"><?php print $analysis->description; ?></div>
<?php  
} ?>

