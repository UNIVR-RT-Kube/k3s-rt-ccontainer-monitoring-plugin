#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/resource.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>

#define PERIOD 0.5

static void sig_thread(int sig);
static void sig_thread_end(int sig);
static void * sleepy_wait(void * arg);
static void * period_send(void * arg);

int sigxcpu_counter = 0;
int sigxcpu_counter_last_sent = 0;
int sigxcpu_counter_difference = 0;
int zero_sent = 0;

int sleepy_wait_continue = 1;

char *command_post = "curl https://${KUBERNETES_SERVICE_HOST}:${KUBERNETES_SERVICE_PORT}/apis/rt.francescol96.univr/v1alpha1/namespaces/default/monitorings/${HOSTNAME}.${NODENAME}.rtmonitorobj --cacert /var/run/secrets/kubernetes.io/serviceaccount/ca.crt --header \"Authorization: Bearer $(cat /var/run/secrets/kubernetes.io/serviceaccount/token)\" -X POST -H 'Content-Type: application/yaml' -d \"---\n"
"apiVersion: rt.francescol96.univr/v1alpha1\n"
"kind: Monitoring\n"
"metadata:\n"
"  name: ${HOSTNAME}.${NODENAME}.rtmonitorobj\n"
"spec:\n"
"  node: ${NODENAME}\n"
"  podname: ${HOSTNAME}\n"
"  missedDeadlinesTotal: 0\n"
"  missedDeadlinesPeriod: 0\" >>/dev/null 2>>/dev/null && exit";

char *command_patch = "curl https://${KUBERNETES_SERVICE_HOST}:${KUBERNETES_SERVICE_PORT}/apis/rt.francescol96.univr/v1alpha1/namespaces/default/monitorings/${HOSTNAME}.${NODENAME}.rtmonitorobj --cacert /var/run/secrets/kubernetes.io/serviceaccount/ca.crt --header \"Authorization: Bearer $(cat /var/run/secrets/kubernetes.io/serviceaccount/token)\" -X PATCH -H 'Content-Type: application/merge-patch+json' -d \'{ \"spec\": { \"missedDeadlinesTotal\": %d, \"missedDeadlinesPeriod\": %d } }\' >>/dev/null 2>>/dev/null && exit";
 
void monitor() {
	signal(SIGXCPU, sig_thread);
	signal(SIGTERM, sig_thread_end);
	pthread_t this_thread;
	pthread_create(&this_thread, NULL, sleepy_wait, NULL);

	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGXCPU);
	sigaddset(&set, SIGTERM);
	sigprocmask(SIG_BLOCK, &set, NULL);
	pthread_create(&this_thread, NULL, period_send, NULL);
}

static void sig_thread(int sig) {
    sigxcpu_counter++;
}

static void * sleepy_wait(void * arg) {
	setpriority(PRIO_PROCESS, 0, 19);
	while(sleepy_wait_continue) {
		sleep(PERIOD);
	}
}

static void * period_send(void * arg) {
	setpriority(PRIO_PROCESS, 0, 19);
	system(command_post);	
	while(sleepy_wait_continue) {
		char command_insert_numbers[2048];
		sigxcpu_counter_difference = sigxcpu_counter-sigxcpu_counter_last_sent;
		if (sigxcpu_counter_difference > 0) {
			zero_sent = 0;
		}
		if (sigxcpu_counter_difference >= 0 && !zero_sent) {
			sprintf(command_insert_numbers, command_patch, sigxcpu_counter, sigxcpu_counter_difference);
			system(command_insert_numbers);	
			sigxcpu_counter_last_sent = sigxcpu_counter;
			if (sigxcpu_counter_difference == 0) {
				zero_sent = 1;
			}
		}
		sleep(PERIOD);
	}
}

static void sig_thread_end(int sig) {
	char command_insert_numbers[2048];
	sprintf(command_insert_numbers, command_patch, sigxcpu_counter, 0);
	system(command_insert_numbers);
	sleepy_wait_continue = 0;
	exit(1);
}
