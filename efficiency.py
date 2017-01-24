#!/opt/python-2.7.6/bin/python

import sys
import os
from pylsf import *
import argparse
from operator import itemgetter

def start_lsf_daemon():
    """Initialize connection to LSF daemon."""
    daemon = lsb_init("")
    return

def grab_job_list():
    """Retrieve all running jobs that have updated CPU status (running for longer than some LSF-arbitrary length of time)."""
    bffr = lsb_openjobinfo()

    jobs = []

    for entry in range(bffr):
        job = lsb_readjobinfo(entry)
        # test if job has been running longer than one LSF update cycle (-1s have been updated internally)
        if job_status(job[2]) == "RUN" and all(item > 0 for item in [job[0], job[43][2], job[43][3], job[45], job[19][5]]):
            jobs.append(job)

    return jobs

def compute_efficiency(job):
    """Compute efficiency value of running job using the formula:
    
    effiency = (utime + stime) / runTime * numProcessors)

    as pulled from the API. cpuTime not used because LSF apparently does not update the value until the job finishes.
    """
    efficiency = float((job[43][2] + job[43][3]) / float((job[45] * job[19][5])))

    return efficiency

def efficiency_summary(list_of_lists):
    """count number of offending jobs per userid, and average efficiency"""
    summary_table = []

    for job in list_of_lists:
        if job[0] !="USERID" and (job[0] not in [entry[0] for entry in summary_table]):
            summary_table.append([job[0], None, None, None])

    for idx, user_entry in enumerate(summary_table):
        # count
        summary_table[idx][1] = str(sum(row.count(user_entry[0]) for row in list_of_lists))
        # average
        summary_table[idx][2] = str('%.2f' % float(sum([float(row[2]) for row in list_of_lists if row[0] == user_entry[0]]) / float(summary_table[idx][1])))
        #max
        summary_table[idx][3] = str(max([float(row[2]) for row in list_of_lists if row[0] == user_entry[0]]))

    summary_table = sorted(summary_table, key=itemgetter(2,3), reverse=True)

    summary_table.insert(0, ['USERID', 'COUNT', 'AVERAGE', 'MAX'])

       
    return summary_table

def main(threshold):

    start_lsf_daemon()
    jobs = grab_job_list()

    if jobs:
        header = ["USERID", "JOBID", "EFFICIENCY (>= %s)" % threshold]
        efficiency_list = []

        for job in jobs:
            efficiency_value = compute_efficiency(job)

            if efficiency_value > threshold:
                efficiency_list.append([job[1], str(job[0]), str('%.2f'%efficiency_value)])

        if efficiency_list:
            efficiency_list = sorted(efficiency_list, key=lambda x: (x[0],float(x[2])), reverse=True)

            efficiency_list.insert(0, header)
            efficiency_list.append(header)

            print

            # The following line was adapted from http://stackoverflow.com/questions/9989334/create-nice-column-output-in-python
            widths = [max(map(len, col)) for col in zip(*efficiency_list)]

            for element in efficiency_list:
                print '\t'.join((val.ljust(width + 1) for val, width in zip(element, widths)))

            # End appropriated block

            print

            summary_table = efficiency_summary(efficiency_list)

            # Reusing above formatting block

            summary_widths = [max(map(len, col)) for col in zip(*summary_table)]

            for user_entry in summary_table:
                print '\t'.join((val.ljust(width + 1) for val, width in zip(user_entry, summary_widths)))
     

            print

        else:
            print "No unfinished jobs with efficiency >= %s" % threshold

    else:
        print "no running jobs"


if __name__ == "__main__":
    parser = argparse.ArgumentParser('Display snapshot of list of jobs in pseudoreal-time that are using more than some amount of allocated processes.')
    parser.add_argument('-t', '--threshold', help='efficiency floor value', default=1.1)

    args = parser.parse_args()

    threshold = float(args.threshold)

    main(threshold)
