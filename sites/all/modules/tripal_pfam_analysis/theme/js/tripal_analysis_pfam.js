if (Drupal.jsEnabled) {
   
   $(document).ready(function(){
	   var fontcolor = $(".pfam_results_table tbody tr td table tbody tr td b a").parent().prev().css('color');
	   // Disable the hyperlink for sequences in the pfam box
	   $(".tripal_pfam_results_table tbody tr td table tbody tr td b a").removeAttr('href');
	   $(".tripal_pfam_results_table tbody tr td table tbody tr td b a").css('font-weight','normal');
	   $(".tripal_pfam_results_table tbody tr td table tbody tr td b a").css('text-decoration','none');
	   $(".tripal_pfam_results_table tbody tr td table tbody tr td b a").css('color',fontcolor);
	   
	   // Allow selection of "Load GO term to the database" only when the submitting job to parse
	   // html output is enabled
	   isSubmittingJob ();
   });
   
   // Disable parse GO checkbox if no pfam job is submitted
   function isSubmittingJob () {
	   if ($('#edit-pfamjob').is(":checked")) {
		   var fontcolor = $("#edit-parsego").parent().parent().prev().children().css('color');
		   $("#edit-parsego").attr('disabled', false);
		   $("#edit-parsego-wrapper").css("color", fontcolor);
		   
	   } else {
		   $("#edit-parsego").attr('checked', false);
		   $("#edit-parsego").attr('disabled', true);
		   $("#edit-parsego-wrapper").css("color", "grey");
	   }
   }
}
