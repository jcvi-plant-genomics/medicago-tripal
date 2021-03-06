<?php
/**
 * @file
 * Tripal offers a job management subsystem for managing tasks that may require
 * an extended period of time for completion.
 */

/**
 * @defgroup tripal_jobs_api Tripal Jobs API
 * @ingroup tripal_core_api
 * @{
 * Tripal offers a job management subsystem for managing tasks that may require
 * an extended period of time for completion.  Drupal uses a UNIX-based cron
 * job to handle tasks such as  checking  the  availability of updates, indexing
 * new nodes for searching, etc.   Drupal's cron uses the web interface for
 * launching these tasks, however, Tripal provides several administrative tasks
 * that may time out and not complete due to limitations of the web server.
 * Examples including syncing of a large number of features between chado and
 * Drupal.  To circumvent this, as well as provide more fine-grained control
 * and monitoring, Tripal uses a jobs management sub-system built into the
 * Tripal Core module.   It is anticipated that this functionality will be used
 * for managing analysis jobs provided by future tools, with eventual support
 * for distributed computing.
 *
 * The  Tripal jobs management system allows administrators to submit tasks to
 * be performed which can then  be launched through a UNIX command-line PHP
 * script or cron job.  This command-line script can be added to a cron entry
 * along-side the Drupal cron entry for automatic, regular launching of Tripal
 * jobs.  The order of execution of waiting jobs is determined first by
 * priority and second by the order the jobs were entered.
 *
 * The API functions described below provide a programmatic interface for
 * adding, checking and viewing jobs.
 * @}
 */

/**
 * Adds a job to the Tripal Jbo queue
 *
 * @param $job_name
 *    The human readable name for the job
 * @param $modulename
 *    The name of the module adding the job
 * @param $callback
 *    The name of a function to be called when the job is executed
 * @param $arguments
 *    An array of arguments to be passed on to the callback
 * @param $uid
 *    The uid of the user adding the job
 * @param $priority
 *    The priority at which to run the job where the highest priority is 10 and the lowest priority
 *    is 1. The default priority is 10.
 *
 * @return
 *    The job_id of the registered job
 *
 * Example usage:
 * @code
 *  $args = array($dfile, $organism_id, $type, $library_id, $re_name, $re_uname,
 *        $re_accession, $db_id, $rel_type, $re_subject, $parent_type, $method,
 *         $user->uid, $analysis_id, $match_type);
 *
 * tripal_add_job("Import FASTA file: $dfile", 'tripal_feature',
 *   'tripal_feature_load_fasta', $args, $user->uid);
 * @endcode
 * The code above is copied from the tripal_feature/fasta_loader.php file. The
 * snipped first builds an array of arguments that will then be passed to the
 * tripal_add_job function.  The number of arguments provided in the $arguments
 * variable should match the argument set for the callback function provided
 * as the third argument.
 *
 * @ingroup tripal_jobs_api
 */
function tripal_add_job($job_name, $modulename, $callback, $arguments, $uid, $priority = 10) {
  if (!$job_name) {
    watchdog('tripal', "Must provide a \$job_name argument to the tripal_add_job() function.", 'error');
    return FALSE;
  }
  if (!$modulename) {
    watchdog('tripal', "Must provide a \$modulename argument to the tripal_add_job() function.", 'error');
    return FALSE;
  }
  if (!$callback) {
    watchdog('tripal', "Must provide a \$callback argument to the tripal_add_job() function.", 'error');
    return FALSE;
  }

  if (!function_exists($callback)) {
    watchdog('tripal', "Must provide a valid callback function to the tripal_add_job() function.", 'error');
    return FALSE;
  }
  if (!is_numeric($uid)) {
    watchdog('tripal', "Must provide a numeric \$uid argument to the tripal_add_job() function.", 'error');
    return FALSE;
  }
  if (!$priority or !is_numeric($priority) or $priority < 1 or $priority > 10) {
    watchdog('tripal', "Must provide a numeric \$priority argument between 1 and 10 to the tripal_add_job() function.", 'error');
    return FALSE;
  }
  if (!is_array($arguments)) {
    watchdog('tripal', "Must provide an array as the \$arguments argument to the tripal_add_job() function.", 'error');
    return FALSE;
  }

  $user = user_load($uid);

  // convert the arguments into a string for storage in the database
  $args = array();
  if (is_array($arguments)) {
    $args = serialize($arguments);
  }

  $job_id = db_insert('tripal_jobs')
    ->fields(array(
      'job_name' => $job_name,
      'modulename' => $modulename,
      'callback' => $callback,
      'status' => 'Waiting',
      'submit_date' => time(),
      'uid' => $uid,
      # The lower the number the higher the priority.
      'priority' => $priority,
      'arguments' => $args,
    ))
    ->execute();

  if ($job_id) {
    drupal_set_message(t("Job '%job_name' submitted.", array('%job_name' => $job_name)));
    if (user_access('administer tripal')) {
      $jobs_url = url("admin/tripal/tripal_jobs");
      drupal_set_message(t("Check the <a href='!jobs_url'>jobs page</a> for status.",
        array('!jobs_url' => $jobs_url)));
      drupal_set_message(t("You can execute the job queue manually on the command line " .
        "using the following Drush command: <br>drush trp-run-jobs --username=%uname --root=%base_path",
        array('%base_path' => DRUPAL_ROOT, '%uname' => $user->name)));
    }
  }
  else {
    drupal_set_message(t("Failed to add job: %job_name.", array('%job_name' => $job_name)), 'error');
  }

  return $job_id;
}

/**
 * Retrieve information regarding a tripal job
 *
 * @param $job_id
 *   The unique identifier of the job
 *
 * @return
 *   An object describing the job if a job is found or FALSE on failure.
 *
 * @ingroup tripal_jobs_api
 */
function tripal_get_job($job_id) {
  if (!$job_id or !is_numeric($job_id)) {
    watchdog('tripal', "Must provide a numeric \$job_id to the tripal_cancel_job() function.");
    return FALSE;
  }

  $job = db_query('SELECT j.* FROM {tripal_jobs} j WHERE j.job_id=:job_id', array(':job_id' => $job_id))
    ->fetchObject();

  $job->submit_date_string = format_date($job->submit_date);
  $job->start_time_string = format_date($job->start_time);
  $job->end_time_string = format_date($job->end_time);

  return $job;
}

/**
 * Indicates if any jobs are running.
 *
 * This function will check the system to see if a job has a process ID
 * and if that process ID is still running. It will update the job status
 * accordingly before returning.
 *
 * @return
 *   Returns TRUE if any job is running or FALSE otherwise.
 *
 * @ingroup tripal_jobs_api
 */
function tripal_is_job_running() {

  // Iterate through each job that has not ended
  // and see if it is still running. If it is not
  // running but does not have an end_time then
  // set the end time and set the status to 'Error'
  $sql =  "SELECT * FROM {tripal_jobs} TJ " .
          "WHERE TJ.end_time IS NULL and NOT TJ.start_time IS NULL ";
  $jobs = db_query($sql);
  foreach ($jobs as $job) {
    $status = shell_exec('ps -p ' . escapeshellarg($job->pid) . ' -o pid=');
    if ($job->pid && $status) {
      // the job is still running so let it go
      // we return 1 to indicate that a job is running
      return TRUE;
    }
    else {
      // the job is not running so terminate it
      $record = new stdClass();
      $record->job_id = $job->job_id;
      $record->end_time = time();
      $record->status = 'Error';
      $record->error_msg = 'Job has terminated unexpectedly.';
      drupal_write_record('tripal_jobs', $record, 'job_id');
    }
  }

  // return 1 to indicate that no jobs are currently running.
  return FALSE;
}

/**
 * Returns the start time for a given job
 *
 * @param $job
 *   An object describing the job
 *
 * @return
 *   The start time of the job if it was already run and either "Cancelled" or "Not Yet Started" otherwise
 *
 * @ingroup tripal_jobs_api
 */
function tripal_get_job_start($job) {

  if ($job->start_time > 0) {
    $start = format_date($job->start_time);
  }
  else {
    if (strcmp($job->job_status, 'Cancelled')==0) {
      $start = 'Cancelled';
    }
    else {
      $start = 'Not Yet Started';
    }
  }
  return $start;
}

/**
 * Returns the end time for a given job
 *
 * @param $job
 *   An object describing the job
 *
 * @return
 *   The end time of the job if it was already run and empty otherwise
 *
 * @ingroup tripal_jobs_api
 */
function tripal_get_job_end($job) {

  if ($job->end_time > 0) {
    $end = format_date($job->end_time);
  }
  else {
    $end = '';
  }

  return $end;
}


/**
 * Check for too many concurrent jobs
 *
 * @param $max_jobs
 *   The maximum number of concurrent jobs to allow; -1 = no limit
 *
 * @ingroup tripal_jobs_api
 */
function tripal_max_jobs_exceeded($max_jobs) {
  if ($max_jobs < 0) {
    // No limit on concurrent jobs
    return FALSE;
  }

  $num_jobs_running = 0;

  // Iterate through each job that has not ended and see if it is still running.
  // If it is not running but does not have an end_time then set the end time
  // and set the status to 'Error'
  $sql =  "SELECT * FROM {tripal_jobs} TJ " .
          "WHERE TJ.end_time IS NULL and NOT TJ.start_time IS NULL ";
  $jobs = db_query($sql);
  foreach ($jobs as $job) {
    $status = shell_exec('ps -p ' . escapeshellarg($job->pid) . ' -o pid=');
    if ($job->pid && $status) {
      // the job is still running
      $num_jobs_running++;
    }
    else {
      // the job is not running so terminate it
      $record = new stdClass();
      $record->job_id = $job->job_id;
      $record->end_time = time();
      $record->status = 'Error';
      $record->error_msg = 'Job has terminated unexpectedly.';
      drupal_write_record('tripal_jobs', $record, 'job_id');
    }
  }

  return ($num_jobs_running >= $max_jobs);
}


/**
 * Set a job to be re-ran (ie: add it back into the job queue)
 *
 * @param $job_id
 *   The job_id of the job to be re-ran
 * @param $goto_jobs_page
 *   If set to TRUE then after the re run job is added Drupal will redirect to the jobs page
 *
 * @ingroup tripal_jobs_api
 */
function tripal_rerun_job($job_id, $goto_jobs_page = TRUE) {
  global $user;

  $user_id = $user->uid;

  $sql = "SELECT * FROM {tripal_jobs} WHERE job_id = :job_id";
  $results = db_query($sql, array(':job_id' => $job_id));
  $job = $results->fetchObject();
  // arguments for jobs used to be stored as plain string with a double colon
  // separating them.  But as of Tripal v2.0 the arguments are stored as
  // a serialized array.  To be backwards compatible, we should check for serialization
  // and if not then we will use the old style
  $args = unserialize($job->arguments);
  if (!$args) {
    $args = explode("::", $job->arguments);
  }
  $job_id = tripal_add_job($job->job_name, $job->modulename, $job->callback, $args, $user_id, $job->priority);

  if ($goto_jobs_page) {
    drupal_goto("admin/tripal/tripal_jobs");
  }
  return $job_id;
}

/**
 * Cancel a Tripal Job currently waiting in the job queue
 *
 * @param $job_id
 *   The job_id of the job to be cancelled
 *
 * @return
 *   FALSE if the an error occured or the job could not be canceled, TRUE
 *   otherwise.
 *
 * @ingroup tripal_jobs_api
 */
function tripal_cancel_job($job_id, $redirect = TRUE) {

  if (!$job_id or !is_numeric($job_id)) {
    watchdog('tripal', "Must provide a numeric \$job_id to the tripal_cancel_job() function.");
    return FALSE;
  }

  $sql = "SELECT * FROM {tripal_jobs} WHERE job_id = :job_id";
  $results = db_query($sql, array(':job_id' => $job_id));
  $job = $results->fetchObject();

  // set the end time for this job
  if ($job->start_time == 0) {
    $record = new stdClass();
    $record->job_id = $job->job_id;
    $record->end_time = time();
    $record->status = 'Cancelled';
    $record->progress = '0';
    drupal_write_record('tripal_jobs', $record, 'job_id');
    drupal_set_message(t("Job #%job_id cancelled", array('%job_id' => $job_id)));
  }
  else {
    drupal_set_message(t("Job %job_id cannot be cancelled. It is in progress or has finished.", array('%job_id' => $job_id)));
  }
  if ($redirect) {
    drupal_goto("admin/tripal/tripal_jobs");
  }
}


/**
 * Execute a specific Tripal Job.
 *
 * @param $job_id
 *          The job id to be exeuted
 * @param bool $redirect [optional]
 *          Whether to redirect to the job page or not
 */
function tripal_execute_job($job_id, $redirect = TRUE) {
  $sql = "SELECT * FROM {tripal_jobs} WHERE job_id = :job_id";
  $results = db_query($sql, array(':job_id' => $job_id));
  $job = $results->fetchObject();

  // set the end time for this job
  if ($job->start_time == 0 and $job->end_time == 0) {
    tripal_launch_job(1, $job_id);
    drupal_set_message(t("Job %job_id has finished executing. See below for more information.", array('%job_id' => $job_id)));
  }
  else {
    drupal_set_message(t("Job %job_id cannot be executed. It has already finished.", array('%job_id' => $job_id)));
  }

  if ($redirect) {
    drupal_goto("admin/tripal/tripal_jobs/view/$job_id");
  }
}

/**
 * A function used to manually launch all queued tripal jobs
 *
 * @param $do_parallel
 *   A boolean indicating whether jobs should be attempted to run in parallel
 *
 * @param $job_id
 *   To launch a specific job provide the job id.  This option should be
 *   used sparingly as the jobs queue managment system should launch jobs
 *   based on order and priority.  However there are times when a specific
 *   job needs to be launched and this argument will allow it.  Only jobs
 *   which have not been run previously will run.
 * @param $max_jobs
 *   The maximum number of jobs that should be run concurrently. If -1 then unlimited.
 * @param $single
 *   Ensures only a single job is run rather then the entire queue.
 * @ingroup tripal_jobs_api
 */
function tripal_launch_job($do_parallel = 0, $job_id = NULL, $max_jobs = -1, $single = 0) {

  // first check if any jobs are currently running
  // if they are, don't continue, we don't want to have
  // more than one job script running at a time
  if (!$do_parallel and tripal_is_job_running()) {
    print "Jobs are still running. Use the --parallel=1 option with the Drush command to run jobs in parallel.\n";
    return;
  }

  if ($do_parallel && tripal_max_jobs_exceeded($max_jobs)) {
    print "At least $max_jobs jobs are still running. At least one of these jobs much complete before a new job can start.\n";
    return;
  }

  // get all jobs that have not started and order them such that
  // they are processed in a FIFO manner.
  if ($job_id) {
    $sql =  "SELECT * FROM {tripal_jobs} TJ " .
            "WHERE TJ.start_time IS NULL and TJ.end_time IS NULL and TJ.job_id = :job_id " .
            "ORDER BY priority ASC,job_id ASC";
    $job_res = db_query($sql, array(':job_id' => $job_id));
  }
  else {
    $sql =  "SELECT * FROM {tripal_jobs} TJ " .
            "WHERE TJ.start_time IS NULL and TJ.end_time IS NULL " .
            "ORDER BY priority ASC,job_id ASC";
    $job_res = db_query($sql);
  }
  print "There are " . $job_res->rowCount() . " jobs queued.\n";

  foreach ($job_res as $job) {
    // set the start time for this job
    $record = new stdClass();
    $record->job_id = $job->job_id;
    $record->start_time = time();
    $record->status = 'Running';
    $record->pid = getmypid();
    drupal_write_record('tripal_jobs', $record, 'job_id');

    // call the function provided in the callback column.
    // Add the job_id as the last item in the list of arguments. All
    // callback functions should support this argument.
    $callback = $job->callback;

    // arguments for jobs used to be stored as plain string with a double colon
    // separating them.  But as of Tripal v2.0 the arguments are stored as
    // a serialized array.  To be backwards compatible, we should check for serialization
    // and if not then we will use the old style
    $args = unserialize($job->arguments);
    if (!$args) {
      $args = explode("::", $job->arguments);
    }
    $args[] = $job->job_id;

    // We need to do some additional processing for printing since the switch
    // to serialized arrays now allows nested arrays which cause errors when
    // printed using implode alone.
    $string_args = array();
    foreach ($args as $k => $a) {
      if (is_array($a)) {
        $string_args[$k] = 'Array';
      }
      elseif (is_object($a)) {
        $string_args[$k] = 'Object';
      }
      else {
        $string_args[$k] = $a;
      }
    }

    print "Calling: $callback(" . implode(", ", $string_args) . ")\n";
    call_user_func_array($callback, $args);
    // set the end time for this job
    $record->end_time = time();
    $record->status = 'Completed';
    $record->progress = '100';
    drupal_write_record('tripal_jobs', $record, 'job_id');

    if ($single) {
      // Don't start any more jobs
      break;
    }
    if (tripal_max_jobs_exceeded($max_jobs)) {
      break;
    }

    // send an email to the user advising that the job has finished
  }
}

/**
 * An internal function for setting the progress for a current job
 *
 * @param $job_id
 *   The job_id to set the progress for
 * @param $percentage
 *   The progress to set the job to
 *
 * @return
 *   True on success and False otherwise
 *
 * @ingroup tripal_core
 */
function tripal_set_job_progress($job_id, $percentage) {

  if (preg_match("/^(\d+|100)$/", $percentage)) {
    $record = new stdClass();
    $record->job_id = $job_id;
    $record->progress = $percentage;
    if (drupal_write_record('tripal_jobs', $record, 'job_id')) {
      return TRUE;
    }
  }

  return FALSE;
}
/**
 * Returns a list of jobs that are active.
 *
 * @param $modulename
 *   Limit the list returned to those that were added by a specific module. If
 *   no module name is provided then all active jobs are returned.
 *
 * @return
 *    An array of objects where each object describes a tripal job. If no
 *    jobs were found then an empty array is returned.  Each object will have
 *    the following members:
 *    - job_id: The unique ID number for the job.
 *    - uid: The ID of the user that submitted the job.
 *    - job_name:  The human-readable name of the job.
 *    - modulename: The name of the module that submitted the job.
 *    - callback:  The callback function to be called when the job is run.
 *    - arguments: An array of arguments to be passed to the callback function.
 *    - progress: The percent progress of completion if the job is running.
 *    - status: The status of the job: Waiting, Completed, Running or Cancelled.
 *    - submit_date:  The UNIX timestamp when the job was submitted.
 *    - start_time: The UNIX timestamp for when the job started running.
 *    - end_time: The UNIX timestampe when the job completed running.
 *    - error_msg: Any error message that occured during execution of the job.
 *    - prirotiy: The execution priority of the job (value between 1 and 10)
 *
 * @ingroup tripal_jobs_api
 */
function tripal_get_active_jobs($modulename = NULL) {
  $query = db_select('tripal_jobs', 'TJ')
    ->fields('TJ', array('job_id', 'uid', 'job_name', 'modulename', 'callback',
      'arguments', 'progress', 'status', 'submit_date', 'start_time',
      'end_time', 'error_msg', 'priority'));
  if ($modulename) {
    $query->where(
      "TJ.modulename = :modulename and NOT (TJ.status = 'Completed' or TJ.status = 'Cancelled')",
      array(':modulename' => $modulename)
    );
  }
  $results = $query->execute();

  $jobs = array();
  while($job = $results->fetchobject()) {
    $jobs->arguments = unserialize($job->arguments);
    $jobs[] = $job;
  }
  return $jobs;
}

/**
 * Returns the date the job was added to the queue
 *
 * @param $job
 *   An object describing the job
 *
 * @return
 *   The date teh job was submitted
 *
 * @ingroup tripal_jobs_api
 */
function tripal_get_job_submit_date($job) {
  return format_date($job->submit_date);
}
