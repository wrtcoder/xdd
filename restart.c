/* Copyright (C) 1992-2010 I/O Performance, Inc. and the
 * United States Departments of Energy (DoE) and Defense (DoD)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program in a file named 'Copying'; if not, write to
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139.
 */
/* Principal Author:
 *      Tom Ruwart (tmruwart@ioperformance.com)
 * Contributing Authors:
 *       Steve Hodson, DoE/ORNL
 *       Steve Poole, DoE/ORNL
 *       Brad Settlemyer, DoE/ORNL
 * Funding and resources provided by:
 * Oak Ridge National Labs, Department of Energy and Department of Defense
 *  Extreme Scale Systems Center ( ESSC ) http://www.csm.ornl.gov/essc/
 *  and the wonderful people at I/O Performance, Inc.
 */
/*
 * This file contains the subroutines necessary to manage everything to
 * do with restarting an xddcp operation.
 */
#include "xdd.h"

// Prototypes
int xdd_restart_create_restart_file(restart_t *rp);
int xdd_restart_write_restart_file(restart_t *rp);

/*----------------------------------------------------------------------------*/
// This routine is called to create a new restart file when a new copy 
// operation is started for the first time.
// The filename of the restart file can be specified by the "-restart file xxx"
// option where xxx will be the file name of the restart file. 
// If the restart file name is not specified then the default location and 
// name will be used. Currently, the default location is the current working
// directory where xddcp is being executed and the file name will be 
//     xdd.$src.$src_basename.$dest.$dest_basename.$gmt_timestamp-GMT.$ext
// where $src is the host name of the source machine
//       $src_basename is the base name of the source file
//       $dest is the host name of the destination machine
//       $dest_basename is the base name of the destination file
//       $gmt_timestamp is the time at which this restart file was created in
//         the form YYYY-MM-DD-hhmm-GMT or year-month-day-hourminutes in GMT
//         and the "-GMT" is appended to the timestamp to indicate this timezone
//       $ext is the file extension which is ".rst" for this type of file
//
int 
xdd_restart_create_restart_file(restart_t *rp) {

	time_t	t;				// Time structure
	struct 	tm	*tm;		// Pointer to the broken-down time struct that lives in the restart struct
	
 
	// Check to see if the file name was provided or not. If not, the create a file name.
	if (rp->restart_filename == NULL) { // Need to create the file name here
		rp->restart_filename = malloc(MAX_TARGET_NAME_LENGTH);
		if (rp->restart_filename == NULL) {
			fprintf(xgp->errout,"%s: RESTART_MONITOR: ALERT: Cannot allocate %d bytes of memory for the restart file name\n",
				xgp->progname,
				MAX_TARGET_NAME_LENGTH);
			perror("Reason");
			rp->fp = xgp->errout;
			return(0);
		}
		// Get the current time in a appropriate format for a file name
		time(&t);
		tm = gmtime_r(&t, &rp->tm);
		sprintf(rp->restart_filename,"xdd.%s.%s.%s.%s.%4d-%02d-%02d-%02d%02d-GMT.rst",
			(rp->source_host==NULL)?"NA":rp->source_host,
			(rp->source_filename==NULL)?"NA":basename(rp->source_filename),
			(rp->destination_host==NULL)?"NA":rp->destination_host,
			(rp->destination_filename==NULL)?"NA":basename(rp->destination_filename),
			tm->tm_year+1900, // number of years since 1900
			tm->tm_mon+1,  // month since January - range 0 to 11 
			tm->tm_mday,   // day of the month range 1 to 31
			tm->tm_hour,
			tm->tm_min);
	}

	// Now that we have a file name lets try to open it in purely write mode.
	rp->fp = fopen(rp->restart_filename,"w");
	if (rp->fp == NULL) {
		fprintf(xgp->errout,"%s: RESTART_MONITOR: ALERT: Cannot create restart file %s!\n",
			xgp->progname,
			rp->restart_filename);
		perror("Reason");
		fprintf(xgp->errout,"%s: RESTART_MONITOR: ALERT: Defaulting to error out for restart file\n",
			xgp->progname);
		rp->fp = xgp->errout;
		rp->restart_filename = NULL;
		free(rp->restart_filename);
		return(-1);
	}
	
	// Success - everything must have worked and we have a restart file
	fprintf(xgp->output,"%s: RESTART_MONITOR: INFO: Successfully created restart file %s\n",
		xgp->progname,
		rp->restart_filename);
	return(0);
}
/*----------------------------------------------------------------------------*/
// This routine is called to write new information to an existing restart file 
// during a copy operation - this is also referred to as a "checkpoint"
// operation. Each time the write is performed to this file it is flushed to
// the storage device. 
// 
int
xdd_restart_write_restart_file(restart_t *rp) {
	int		status;

	// Seek to the beginning of the file 
	status = fseek(rp->fp, 0L, SEEK_SET);
	if (status < 0) {
		fprintf(xgp->errout,"%s: RESTART_MONITOR: WARNING: Seek to beginning of restart file %s failed\n",
			xgp->progname,
			rp->restart_filename);
		perror("Reason");
	}
	
	// Issue a write operation for the stuff
	fprintf(rp->fp,"-restart offset %llu\n", 
		(unsigned long long int)rp->last_committed_location);

	// Flush the file for safe keeping
	fflush(rp->fp);

	return(SUCCESS);
}
/*----------------------------------------------------------------------------*/
// This routine is created when xdd starts a copy operation (aka xddcp).
// This routine will run in the background and waits for various xdd I/O
// qthreads to update their respective counters and enter the barrier to 
// notify this routine that a block has been read/written.
// This monitor runs on both the source and target machines during a copy
// operation as is indicated in the name of the restart file. The information
// contained in the restart file has different meanings depending on which side
// of the copy operation the restart file represents. 
// On the source side, the restart file contains information regarding the lowest 
// and highest byte offsets into the source file that have been successfully 
// "read" and "sent" to the destination machine. This does not mean that the 
// destination machine has actually received the data but it is an indicator of 
// what the source side thinks has happened.
// On the destination side the restart file contains information regarding the
// lowest and highest byte offsets and lengths that have been received and 
// written (committed) to stable media. This is the file that should be used
// for an actual copy restart opertation as it contains the most accurate state
// of the destination file.
//
// This routine will also display information about the copy operation before,
// during, and after xddcp is complete. 
// 
void *
xdd_restart_monitor(void *junk) {
	int		target_number;
	ptds_t	*current_ptds;
	ptds_t	*current_qthread;
	int32_t low_qthread_number;
	int32_t high_qthread_number;
	uint64_t low_byte_offset;
	uint64_t high_byte_offset;
	uint64_t separation;
	uint64_t check_counter;
	int64_t high_op_number, low_op_number;

	

	// Initialize stuff
	fprintf(xgp->output,"%s: RESTART_MONITOR: Initializing...\n", xgp->progname);
	for (target_number=0; target_number < xgp->number_of_targets; target_number++) {
		current_ptds = xgp->ptdsp[target_number];
		if (current_ptds->target_options & TO_E2E_DESTINATION) {
			xdd_restart_create_restart_file(current_ptds->restartp);
		} else {
			fprintf(xgp->output,"%s: RESTART_MONITOR: INFO: No restart file being created for target %d [ %s ] because this is not the destination side of an E2E operation.\n", 
				xgp->progname,
				current_ptds->my_target_number,
				current_ptds->target);
		}
	}
	fprintf(xgp->output,"%s: RESTART_MONITOR: Initialization complete.\n", xgp->progname);

	// Enter this barrier and wait for the restart monitor to initialize
	xdd_barrier(&xgp->restart_initialization_barrier);

	check_counter = 0;
	// This is the loop that periodically checks all the targets/qthreads 
	for (;;) {
		// Sleep for the specified period of time
		sleep(xgp->restart_frequency);

		check_counter++;
		// Check all targets
		for (target_number=0; target_number < xgp->number_of_targets; target_number++) {
			current_ptds = xgp->ptdsp[target_number];
			// If this target does not require restart monitoring then continue
			if ( !(current_ptds->target_options & TO_RESTART_ENABLE) ) // if restart is NOT enabled for this target then continue
				continue;
			current_qthread = current_ptds;
			// Check all qthreads on this target
			high_byte_offset = 0;
			high_op_number = 0;
			low_byte_offset = current_qthread->last_committed_location;
			low_op_number = current_qthread->last_committed_op;
			low_qthread_number = high_qthread_number = current_qthread->my_qthread_number;
			while (current_qthread) {
#ifdef DEBUG
				fprintf(stderr, "QThread %d last_committed_location is: %llu\n", current_qthread->my_qthread_number, (unsigned long long int)current_qthread->last_committed_location);
#endif
				if ((current_qthread->my_current_state & CURRENT_STATE_COMPLETE) && // if this target has completed all I/O AND the restart monitor has checked it the continue
					(current_qthread->my_current_state & CURRENT_STATE_RESTART_COMPLETE)) {
					current_qthread = current_qthread->nextp;
					continue;
				}

				// At this point this qthread needs to be checked. If it had finished but had not been checked then set the flag saying it has now been checked
				if (current_qthread->my_current_state & CURRENT_STATE_COMPLETE) // If this target has finished then mark the Restart Complete as well
					current_qthread->my_current_state |= CURRENT_STATE_RESTART_COMPLETE;
	
				// If information has not changed since last time, just increment the montior count and continue
                   //////tbd //////
				// If information has changed then set the monitor count to 0, save the byte offset, and continue
				if ( current_qthread->last_committed_location < low_byte_offset) {
					low_byte_offset = current_qthread->last_committed_location; // the new low_byte_offset
					low_qthread_number = current_qthread->my_qthread_number;
					low_op_number = current_qthread->last_committed_op;
				}
				if ( current_qthread->last_committed_location > high_byte_offset) {
					high_byte_offset = current_qthread->last_committed_location; // the new high_byte_offset
					high_qthread_number = current_qthread->my_qthread_number;
					high_op_number = current_qthread->last_committed_op;
				}
				current_qthread = current_qthread->nextp;
			} // End of WHILE loop that looks at all qthreads for this target

#ifdef DEBUG
			fprintf(stderr, "\n>>>> Lowest  QThread %d last_committed_location is: %llu  operation# %lld <<<<\n", 
				low_qthread_number, 
				(unsigned long long int)low_byte_offset,
				(long long int)low_op_number);
			fprintf(stderr, ">>>> Highest QThread %d last_committed_location is: %llu  operation# %lld <<<<\n", 
				high_qthread_number, 
				(unsigned long long int)high_byte_offset,
				(long long int)high_op_number);
			separation = high_byte_offset - low_byte_offset;
			fprintf(stderr, ">>>> Separation is %llu  bytes [ %llu KBytes, %llu MBytes, %llu GBytes] or %lld ops <<<<\n\n", 
				(unsigned long long int)separation,
				(unsigned long long int)(separation/1024),
				(unsigned long long int)(separation/(1024*1024)),
				(unsigned long long int)(separation/(1024*1024*1024)),
				(long long int)(high_op_number - low_op_number));
#endif
			
			// Only need to sync the write data if direct I/O is disabled.  Note that
			// the sync may get starved under Linux if writes occur rapidly enough.
			if (!(current_ptds->target_options & TO_DIO))
			    fdatasync(xgp->ptdsp[target_number]->fd);

			// Now that we have all the information for this target's qthreads, generate the appropriate information
			// and write it to the restart file and sync sync sync
			current_ptds->restartp->last_committed_location = low_byte_offset;
			current_ptds->restartp->low_byte_offset = low_byte_offset;
			if (current_ptds->target_options & TO_E2E_DESTINATION) // Restart files are only written on the destination side
				xdd_restart_write_restart_file(current_ptds->restartp);

		} // End of FOR loop that checks all targets 
		// Done checking all targets

		// If it is time to leave then leave - the qthread cleanup will take care of closing the restart files
                if (xgp->abort_io | xgp->restart_terminate) 
                    break;
	} // End of FOREVER loop that checks stuff
	fprintf(xgp->output,"%s: RESTART Monitor is exiting\n",xgp->progname);
	return(0);
}

/*
 * Local variables:
 *  indent-tabs-mode: t
 *  c-indent-level: 8
 *  c-basic-offset: 8
 * End:
 *
 * vim: ts=8 sts=8 sw=8 noexpandtab
 */
