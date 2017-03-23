<?php
$feature = $variables['node']->feature;
$terms = $feature->tripal_analysis_go->go_terms;

if(count($terms) > 0){ ?>
  <div id="tripal_feature-go_terms-box" class="tripal_feature-info-box tripal-info-box">
    <div class="tripal_feature-info-box-title tripal-info-box-title">GO Assignments</div>
    <div class="tripal_feature-info-box-desc tripal-info-box-desc">This <?php print $feature->type_id->name ?> is annotated with the following GO terms.</div>
    <table id="tripal_feature-go_terms-table" class="tripal_feature-table tripal-table tripal-table-horz">
      <tr>
        <th>Category</th>
        <th>Term Accession</th>
        <th>Term Name</th>
      </tr>
      <?php
      $i = 0;
      foreach ($terms as $term){
        $class = 'tripal_feature-table-odd-row tripal-table-odd-row';
        if($i % 2 == 0 ){
           $class = 'tripal_feature-table-odd-row tripal-table-even-row';
        }
        ?>
        <tr class="<?php print $class ?>">
          <td><?php print $term->cvname ?></td>
          <td><a href="http://amigo.geneontology.org/amigo/term/GO:<?php print $term->accession ?>"  target="_go">GO:<?php print $term->accession?></a></td>
          <td><span title="<?php print $term->definition ?>"><?php print $term->goterm ?></span></td>
        </tr>
        <?php
        $i++;
      } ?>
    </table>
  </div><?php
}
