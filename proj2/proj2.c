#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <signal.h>
#define SEM1_Jmeno "/xmatya11_zastavka_vstup1"
#define SEM2_Jmeno "/xmatya11_autobus_dojel1"
#define SEM3_Jmeno "/xmatya11_autobus1"
#define SEM4_Jmeno "/xmatya11_konec_nastupu1"
#define SEM5_Jmeno "/xmatya11_tisk_sem1"
#define SEM6_Jmeno "/xmatya11_konec_nastupu2"

void init();
void clean();
int isnum();
void tisk();
int hlavni_proces();
void pomocny_rider_maker();
void autobus_fce();
void rider_fce();

sem_t *vstup_zastavka = NULL; //kontrola vstupu na zastavku
sem_t *autobus_dojel = NULL;  //kontrola dokonceni kola autobusu
sem_t *autobus = NULL;		  //prijezd autobusu na zastavku
sem_t *konec_nastupu = NULL;  //ukonceni nastupu rideru
sem_t *tisk_sem= NULL;
sem_t *konec_nastupu2 = NULL; 

int *porad_cislo=NULL;//poradi vykonanych tisku
int porad_cislo_id=0; 
int *cekajici=NULL;  //pocet cekajicich rideru na zastavce
int cekajici_id=0;
int *rider_cislo = NULL; //ocislovani rideru

FILE *output;
	
int main(int argc,char* argv[]){
	//clean();
	if (argc == 5){ //kontrola zadanych argumentu
		if ((isnum(argv[1]))&&(isnum(argv[2]))&&(isnum(argv[3]))&&(isnum(argv[4]))){
			int R =atoi(argv[1]);
			int C =atoi(argv[2]);
			int ART =atoi(argv[3]);
			int ABT =atoi(argv[4]);
			if((R>0)&&(C>0)&&((ART>=0)&&(ART<=1000))&&((ABT>=0)&&(ABT<=1000))){
				hlavni_proces(R,C,ART,ABT);
			}else{
				fprintf(stderr,"Neplatna hodnota argumentu\n");
				exit(1);
			}
		}else{
			fprintf(stderr,"Neplatna hodnota argumentu\n");
			exit(1);
		}
	}
	else{
		fprintf(stderr,"Spatny pocet argumentu\n");
		exit(1);
	}
    exit(0);
}
void init(){ //inicializace potrebnych zdroju
	if ((vstup_zastavka = sem_open(SEM1_Jmeno,O_CREAT, 0666, 1)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	}
	if ((autobus_dojel = sem_open(SEM2_Jmeno,O_CREAT, 0666,  0)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	}
	if ((autobus = sem_open(SEM3_Jmeno,O_CREAT, 0666, 0)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	}
	if ((konec_nastupu = sem_open(SEM4_Jmeno,O_CREAT, 0666, 0)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	}
 	if ((tisk_sem = sem_open(SEM5_Jmeno,O_CREAT, 0666, 1)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	} 	
	if ((konec_nastupu2 = sem_open(SEM6_Jmeno,O_CREAT, 0666, 0)) == SEM_FAILED)
  	{
		fprintf(stderr,"Chyba tvorby semaforu\n");
		exit(1);
 	}
	if ((porad_cislo_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) == -1)
    {
        fprintf(stderr,"Chyba tvorby sdilene pameti\n");
		exit(1);
    }

    if ((porad_cislo = shmat(porad_cislo_id, NULL, 0)) == NULL)
    {
        fprintf(stderr,"Chyba tvorby sdilene pameti\n");
		exit(1);
    } 	
    if ((cekajici_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666)) == -1)
    {
        fprintf(stderr,"Chyba tvorby sdilene pameti\n");
		exit(1);
    }

    if ((cekajici = shmat(cekajici_id, NULL, 0)) == NULL)
    {
        fprintf(stderr,"Chyba tvorby sdilene pameti\n");
		exit(1);
    }
    if ((rider_cislo = malloc(sizeof(int))) == NULL)
    {
        fprintf(stderr,"Chyba alokace pameti\n");
		exit(1);
    }

    output=fopen("proj2.out","w");
    setbuf(output, NULL);
    *porad_cislo = 1;
    *rider_cislo = 0;
}
void clean(){ //uklid pouzitych zdroju
    free(rider_cislo);
    shmctl(porad_cislo_id, IPC_RMID, NULL);
    shmctl(cekajici_id, IPC_RMID, NULL);

    fclose(output);

    sem_close(vstup_zastavka);
    sem_close(autobus_dojel);
    sem_close(autobus);
    sem_close(konec_nastupu);
    sem_close(konec_nastupu2);
    sem_close(tisk_sem);

    sem_unlink(SEM1_Jmeno);
    sem_unlink(SEM2_Jmeno);
    sem_unlink(SEM3_Jmeno);
    sem_unlink(SEM4_Jmeno);
    sem_unlink(SEM5_Jmeno);
    sem_unlink(SEM6_Jmeno);
}
int isnum(char* str){//kontrola jestli zadany string je cislo
	int delka=strlen(str);
	for(int i=0;i<delka;i++){
		if (!isdigit(str[i]))
		{
		return 0;
	        }
	}
	return 1; 
}
void tisk(char* name,int i,char* msg,int cr){ //tisk s hlidanim poradi sdilene pameti
	sem_wait(tisk_sem);
	if((i==-1)&&(cr==-1))
	{
		fprintf(output,"%d   : %s      :%s\n",(*porad_cislo)++,name,msg);	
	}
	else if((i==-1)&&(cr!=-1)){
		fprintf(output,"%d   : %s      :%s: %d\n",(*porad_cislo)++,name,msg,cr);
		//print bez i s cr
	}
	else if((i!=-1)&&(cr==-1)){
		fprintf(output,"%d   : %s %d    :%s\n",(*porad_cislo)++,name,i,msg);
		//print s i bez cr

	}else{
		fprintf(output,"%d   : %s %d    :%s: %d\n",(*porad_cislo)++,name,i,msg,cr);
		//print s i s cr
	}
	sem_post(tisk_sem);
}
void pomocny_rider_maker(int r,int ART){ //funkce pro tvorbu procesu RID
	int prodleva=rand()%(ART+1)*1000;
	for(int i=0;i<r;i++){
		usleep(prodleva);
		(*rider_cislo)++;
	    pid_t RID = fork();
	    if(RID == 0)
	    {
	    	rider_fce((*rider_cislo));	
	    }
	}
    exit(EXIT_SUCCESS);
}
void autobus_fce(int kapacita,int ABT,int r){ //funkce ridici proces BUS
	tisk("BUS",-1,"start",-1);
	int delka_jizdy=rand()%(ABT+1)*1000; //nastaveni delky jizdy autobusu
	int n=0;
	while(!(((*cekajici)==0)&&(r==0)))
	{
		for(int i=0;i<n;i++){
			sem_wait(konec_nastupu2);
		}
		tisk("BUS",-1,"arrival",-1);
		sem_wait(vstup_zastavka);  //zavreni vstupu na zastavku
		if ((*cekajici)<kapacita)  //nastaveni poctu rideru kteri budou nastupovat
		{
			n=(*cekajici);
		}
		else
		{
			n=kapacita;
		}
		if(n>0)
		{		
			tisk("BUS",-1,"start boarding",(*cekajici));
			for (int i=0;i<n;i++) //nastup rideru do autobusu
			{
				sem_post(autobus);
				sem_wait(konec_nastupu);
			}
			if (((*cekajici)-kapacita)>0){ //zjisteni kolik zustalo na zastavce
				(*cekajici)=(*cekajici)-kapacita;
			}
			else
			{
				(*cekajici)=0;
			}
			tisk("BUS",-1,"end boarding",(*cekajici));
		}
		sem_post(vstup_zastavka); //otevreni vstupu na zastavku
		tisk("BUS",-1,"depart",-1);
		r-=n; //aktualizace poctu existujicich rideru
		usleep(delka_jizdy);
		tisk("BUS",-1,"end",-1);
		for(int i=0;i<n;i++){
			sem_post(autobus_dojel); //oznameni riderum v autobusu ze maji vystoupit
		}
	}
	tisk("BUS",-1,"finish",-1);
}
void rider_fce(int rid_id){ //funkce ridici procesi RID
	tisk("RID",rid_id,"start",-1);
	sem_wait(vstup_zastavka);
		(*cekajici)++; //aktualizace poctu cekajicich rideru na zastavce
	tisk("RID",rid_id,"enter",(*cekajici));
	sem_post(vstup_zastavka);
	sem_wait(autobus);	
	tisk("RID",rid_id,"boarding",-1);
	sem_post(konec_nastupu);//oznameni ze rider nastoupil do autobusu
	sem_wait(autobus_dojel);//cekani nez autobus dokonci aktualni jizdu	
	sem_post(konec_nastupu2);
	tisk("RID",rid_id,"finish",-1);
	exit(EXIT_SUCCESS);
}
int hlavni_proces(int r,int c,int ART,int ABT){

	srand(1);
 	init();

 	pid_t hlavni = fork();
 	if(hlavni == 0)// Pomocny Rider Maker
 	{
 		pomocny_rider_maker(r,ART);
 	}
 	else if(hlavni > 0) // Autobus
 	{
 		autobus_fce(c,ABT,r);
 	}
 	else
 	{
 		fprintf(stderr, "Chyba tvorby procesu\n");
 		exit(1);
 	}
	clean();
	return 0;
}
