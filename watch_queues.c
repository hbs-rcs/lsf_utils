/*
  queue and percent utilization for shared, contributed and all nodes 
  -u and -q to see utilization or queue information only
   compiled with: 
      gcc -o watch_queues watch_queues.c -I /opt/lsf/7.0/include/ /opt/lsf/7.0/linux2.6-glibc2.3-x86_64/lib/libbat.a /opt/lsf/7.0/linux2.6-glibc2.3-x86_64/lib/liblsf.a -ldl -lnsl 
   greg cavanagh    
   12 December 2012
*/

/* IMPORTANT NOTES
 *
 * Jobs in testing queues aren't counted towards most totals
 */

/* TODO
 * Add line for "special"? training, mpi_testing, ...
 * Change shared_hosts to shared_queues, because that's what we measure
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "/opt/lsf/7.0/include/lsf/lsf.h"
#include "/opt/lsf/7.0/include/lsf/lsbatch.h"

int is_string_in_array(char *query, int nmatches, char **matches);

int main (argc, argv)
    int  argc;
    char *argv[];
{

    /* bqueues related variables */
    struct queueInfoEnt *qInfo;
    int  numQueues = 0;
    char **all_queues;
    int  i,j,c;
    int TOTAL_JOBS,TOTAL_SUSP,TOTAL_RUN,TOTAL_PEND,TOTAL_SLOTS,TOTAL_PERCENT;
    int SHARED_RUN,SHARED_SLOTS,SHARED_PERCENT;
    int CONTRIBPOOL_RUN,CONTRIBPOOL_SLOTS,CONTRIBPOOL_PERCENT;
    int SPECIALPOOL_RUN,SPECIALPOOL_SLOTS,SPECIALPOOL_PERCENT;
    int DEDICATED_RUN,DEDICATED_SLOTS,DEDICATED_PERCENT;

    char *shared_queues[] = {
      "highmem_unlimited", "interactive", "long", "short", "mini",
      "parallel" , "priority", "no_suspend", "mcore", "mpi", "gpu",
      /* park_shared runs jobs on shared nodes, though it's a dedicated queue */
      "park_shared"
    };
    int num_shared_queues = sizeof(shared_queues)/sizeof(char *);
    /* Queues targeting nodes set aside for a single lab (or set of labs) */
    char *dedicated_queues[] = {
      "i2b2_int_15m", "i2b2_int_2h", "i2b2_int_12h", "i2b2_15m",
	"i2b2_2h", "i2b2_12h", "i2b2_1d", "i2b2_7d", "i2b2_unlimited",
      "bpf_int_12h", "bpf_15m", "bpf_12h", "bpf_unlimited",
      "park_int_15m", "park_int_2h", "park_int_12h", "park_15m", "park_2h",
	"park_12h", "park_1d", "park_7d", "park_unlimited",
      "reich",
      "seidman",
      "cryoem_parallel",
    };
    int num_dedicated_queues = sizeof(dedicated_queues)/sizeof(char *);
    /* extra-special stuff */
    char *special_queues[] = { "training", "mpi_testing", "tmp_unlimited", "admin_nopreempt" };
    int num_special_queues = sizeof(special_queues)/sizeof(char *);
    
    /* bhost related variables */
    struct hostInfoEnt *hInfo;
    char *hostname ;
    int numHosts;
    int show_time,show_queues,show_memory,show_utilization,show_dependency;
    /* Shared hosts anyone can use */
    char *shared[] = {
	/* Note: As of 201607, shared_all is different than shared_hosts
	 *   because it doesn't include shared_parallel3 */
      "shared_hosts",
      "highmem" /* Not EVERYONE can use these, but they're not dedicated */
    };
    int num_shared_groups = sizeof(shared)/sizeof(char *);
    /* Set of nodes that used to be contributed by individual faculty.
     * Now a whole bunch of seldom-used queues targets this small batch */
    char *contrib[] = { "contrib1", "contrib2" };
    int num_contrib_groups = sizeof(contrib)/sizeof(char *);
    /* Sets of nodes dedicated to individual (groups of) labs */
    char *dedicated[] = {
      "bpf", "i2b2", "park", "reich", "seidman", "cryoem_parallel"
    };
    int num_dedicated_groups = sizeof(dedicated)/sizeof(char *);
    /* nodes to ignore even for the overall total */
    char *ignore[] = {
      "fife000-156" /* test node */
      , "clarinet001-063" , "clarinet001-064" , "clarinet001-065" /* TODO evfold group */
    };
    int num_ignore_groups = sizeof(ignore)/sizeof(char *);

    time_t mytime;

    mytime = time(NULL);
    show_time=show_queues=show_memory=show_utilization=show_dependency=1;

    /* parse arguments */
    while ((c = getopt (argc, argv, "hquDt")) != -1){
      switch (c)
	{
	case 'h':
	    printf("\nUsage: watch_queues [-h|-q|-u|-t]\n"
		"\t-h\tShow this usage\n"
		"\t-q\tShow only queues (like bqueues for only queues with jobs)\n"
		"\t-t\tTurn OFF date/timestamp\n"
		"\t-u\tShow only utilization percentages\n\n");
	    exit(0);
        case 't':
          show_time=0;
          break;
	case 'u':
	  show_queues=show_memory=show_dependency=0;
	  break;
	case 'D': /* show_dependency. Not currently implemented */
	  show_queues=show_memory=show_utilization=0;
	  break;
	case 'q':
	  show_memory=show_utilization=show_dependency=0;
	  break;
	default:
	  printf("urecognized argument \n");
	  exit(1);
	}
    }

    /* initialize variables */
    TOTAL_JOBS = TOTAL_SUSP = TOTAL_RUN = TOTAL_PEND = TOTAL_SLOTS = TOTAL_PERCENT = 0;
    SHARED_RUN = SHARED_SLOTS = SHARED_PERCENT = 0;
    CONTRIBPOOL_RUN = CONTRIBPOOL_SLOTS = CONTRIBPOOL_PERCENT = 0;	
    SPECIALPOOL_RUN = SPECIALPOOL_SLOTS = SPECIALPOOL_PERCENT = 0;	
    DEDICATED_RUN = DEDICATED_SLOTS = DEDICATED_PERCENT = 0;	

    if ( show_time == 1 ){
	/* printf("\n"); */
	printf(ctime(&mytime));
	printf("\n");
    }
	
    if (lsb_init(argv[0]) < 0) {
        lsb_perror("lsb_init()");
        exit(-1);
    }

    /* bqueues equivalent, get infomation on all queues */
    /* see: https://wiki.med.harvard.edu/doc/lsf/api_ref/lsb_queueinfo.html */
    qInfo = lsb_queueinfo(all_queues, &numQueues, NULL, NULL, 0); 
    if (qInfo == NULL) { 
        lsb_perror("lsb_queueinfo()");
        exit(-1);
    }

    if ( show_queues == 1 ){
      printf("%-20s\t\tNJOBS\tPEND\tRUN\tSUSP\n","QUEUE_NAME");
      printf("-------------------------------------------------------------------\n");
    }
    /* loop over all queues in qInfo */
    for(j = 0; j < numQueues; j++) {	

      /* print if queue has jobs in it */
      int qjobs = qInfo[j].numJobs;
      int qrunning = qInfo[j].numRUN;
      char *qname = qInfo[j].queue;
      if (qjobs != 0 && show_queues == 1 ) {
	  printf("%-20s\t\t%-d\t%-d\t%-d\t%-d\n", qname, qjobs,
		 qInfo[j].numPEND, qInfo[j].numRUN, qInfo[j].numSSUSP + qInfo[j].numUSUSP); 
      }

      /* contrib is everthing that is not the below things */
      /* shared queues: highmem_unlimited interactive long short mini parallel priority no_suspend mcore mpi*/
      /* dedicated_queues: queues for just one group of people */
      /* internal queues: tmp_unlimited training testing */
      /* shared queue running job totals */
      /* note: !strcmp returns true when it matches "value" */
      if (is_string_in_array(qname, num_shared_queues, shared_queues)) {
	  SHARED_RUN = SHARED_RUN + qrunning;                 
      }

	  /* dedicated  queue running job totals , not shared  and not contrib and not internal*/
      else if (is_string_in_array(qname, num_dedicated_queues, dedicated_queues)) {
	  DEDICATED_RUN = DEDICATED_RUN + qrunning;                 
      }

      else {

	/* Ignore special stuff */
	if (is_string_in_array(qname, num_special_queues, special_queues)) {
	  SPECIALPOOL_RUN  = SPECIALPOOL_RUN + qrunning;
	}

	/* contributed queue running job totals , not shared and not internal*/
	else{
	  CONTRIBPOOL_RUN  = CONTRIBPOOL_RUN + qrunning;
	}
	
      }

      /* totals for all queues */
      TOTAL_JOBS = TOTAL_JOBS + qjobs;
      TOTAL_RUN = TOTAL_RUN + qInfo[j].numRUN;
      TOTAL_PEND = TOTAL_PEND + qInfo[j].numPEND;
      TOTAL_SUSP = TOTAL_SUSP + qInfo[j].numSSUSP + qInfo[j].numUSUSP;
      
      
    } /* end loop over all queues in qInfo */

    if ( show_queues == 1 ){
      printf("-------------------------------------------------------------------\n");
      printf("%-20s\t\t%-d\t%-d\t%-d\t%-d\n","TOTALS",TOTAL_JOBS,TOTAL_PEND,TOTAL_RUN,TOTAL_SUSP);
    }


  /* calculate and print the percent utilization of the job slots */
  if ( show_utilization == 1 ){
  
    /* bhost equivalent to get total of shared job slots */
    /* see: https://wiki.med.harvard.edu/doc/lsf/api_ref/lsb_hostinfo.html */
    /* from lsb_hostinfo man page:
     * lsb_hostinfo(hostgroup_array, &numHosts)
     * numHosts has number of hosts or host groups in the hostgroup array
     * (Special case: numHosts=0 returns ALL hosts. hostgroup array ignored)
     * On return, numHosts has number of hosts found
     */
    numHosts = num_shared_groups;
    hInfo = lsb_hostinfo(shared, &numHosts);
    if (hInfo == NULL) { lsb_perror("lsb_hostinfo() failed on shared"); exit (-1); }
    /* loop over hosts, exclude unavail state */
    for(j = 0; j < numHosts; j++) {
      if(is_available(hInfo, j))
        /* printf("%s\tshared\n", hInfo[j].host), */
	SHARED_SLOTS = SHARED_SLOTS + hInfo[j].maxJobs;
    }

    /* all hosts total job slots
     * But ignore nodes that aren't really running LSF jobs
     */
    numHosts=0; /* special case: get ALL hosts. Hostname doesn't matter */
    hInfo = lsb_hostinfo(NULL, &numHosts);
    if (hInfo==NULL) {lsb_perror("lsb_hostinfo() failed on 'all'"); exit (-1);}
    for(j = 0; j < numHosts; j++) {
      char hostname[100];
      strcpy(hostname, hInfo[j].host);
      /*printf("%s\n", hostname); */
      strtok(hostname, "."); /* Get rid of .orchestra if necessary */
      /*printf("%s\n", hostname); */

      if (
	  ! is_string_in_array(hostname, num_ignore_groups, ignore)
	  && is_available(hInfo, j)
      )
        /* printf("%s\ttotal\n", hInfo[j].host), */
	TOTAL_SLOTS = TOTAL_SLOTS + hInfo[j].maxJobs;
    }

    /* contributed job slots */
    numHosts=num_contrib_groups;
    hInfo = lsb_hostinfo(contrib, &numHosts);
    if (hInfo==NULL) {lsb_perror("lsb_hostinfo() failed on contrib");exit (-1);}
    for(j = 0; j < numHosts; j++) {
      if(is_available(hInfo, j))
        /* printf("%s\tcontrib\n", hInfo[j].host), */
	CONTRIBPOOL_SLOTS = CONTRIBPOOL_SLOTS + hInfo[j].maxJobs;
    }

    /* dedicated job slots */
    numHosts = num_dedicated_groups;
    hInfo = lsb_hostinfo(dedicated, &numHosts);
    if(hInfo==NULL){lsb_perror("lsb_hostinfo() failed on dedicated"); exit(-1);}
    for(j = 0; j < numHosts; j++) {
      if(is_available(hInfo, j))
        /* printf("%s\tdedicated\n", hInfo[j].host), */
	DEDICATED_SLOTS = DEDICATED_SLOTS + hInfo[j].maxJobs;
    }

  
    printf("\n%-30s%10s%16s\n","HOST_GROUP","JOBS/SLOTS","UTILIZATION");
    printf("-------------------------------------------------------------------\n");
    printf("%-30s%4d%16s\n","train,test,adm qs (all hosts)", SPECIALPOOL_RUN,"N/A");
    printf("%-30s%4d/%4d%12.4g\%\n","shared_hosts", SHARED_RUN,SHARED_SLOTS,((double)SHARED_RUN)/SHARED_SLOTS*100);
    printf("%-30s%4d/%4d%12.4g\%\n","contrib_hosts", CONTRIBPOOL_RUN,CONTRIBPOOL_SLOTS,((double) CONTRIBPOOL_RUN)/CONTRIBPOOL_SLOTS*100);
    /*printf("%-30s%4d/%4d%12.4g\%\n","bpf,i2b2,park,reich,seidman", DEDICATED_RUN,DEDICATED_SLOTS,((double) DEDICATED_RUN)/DEDICATED_SLOTS*100);*/
    printf("%-30s%4d/%4d%12.4g\%\n","dedicated_hosts", DEDICATED_RUN,DEDICATED_SLOTS,((double) DEDICATED_RUN)/DEDICATED_SLOTS*100);
    printf("%-30s%4d/%4d%12.4g\%\n","hms_orchestra", TOTAL_RUN,TOTAL_SLOTS,((double)TOTAL_RUN)/TOTAL_SLOTS*100);

    /* DEBUG */
    int subsum = SHARED_SLOTS + CONTRIBPOOL_SLOTS + DEDICATED_SLOTS;
    /*printf("DEBUG -- %d total slots, %d subtotal sum, %d difference\n", TOTAL_SLOTS, subsum, TOTAL_SLOTS - subsum); */
  } /* end show utilization */ 
  

  exit(0);

}

/* Does query match any of the N strings in an array of possible matches? */
int is_string_in_array(char *query, int nmatches, char **matches) {
  int j;
  for(j = 0; j < nmatches; j++) {
    if (! strcmp(query, matches[j])) { /* ! strcmp means strings are equal! */
	return 1;
    }
  }
  return 0;
}

int is_available(struct hostInfoEnt *hInfo, int j) {
    return ! (hInfo[j].hStatus & ( HOST_STAT_UNAVAIL | HOST_CLOSED_BY_ADMIN ));
}
