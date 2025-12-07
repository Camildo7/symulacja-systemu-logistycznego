#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>


// Struktura wiadomości
struct message {
    long mtype;  // Typ wiadomości
    int order_id;  // ID zamówienia
    int items[3];  // Liczba surowców (A, B, C)
    int courier_id;  // ID kuriera (tylko dla odpowiedzi)
    int warehouse_id;  // ID magazynu (tylko dla odpowiedzi)
    int is_processed;  // Status zamówienia (tylko dla odpowiedzi)
    int GLD_do_zaplacenia;
    int czy_dostepny_numer_magazynu;

};

struct komunikacja{
    long mtype;
    int czy_koniec;
};

int losuj_liczbe(int n) {
    // Inicjalizacja generatora liczb pseudolosowych
    srand(clock());

    // Losowanie liczby z przedziału <0, n>
    int liczba = rand() % (n + 1);


    return liczba;
}

int main(int argc,char* argv[]){

    //Sprawdzam czy jest odpowiednia liczba argumentow
    if (argc != 6){
        printf("Bledna ilosc argumentow. Przy uruchamianiu podaj 5 argumentow (<klucz> <liczba_zamowien> <max_A> <max_B> <max_C>)\n");
        exit(1);
    }

    int key = 0, liczba_zamowien=0, max_A = 0, max_B = 0, max_C = 0;
    if (atoi(argv[1])!=0) {
        // Jeśli pierwszy argument jest liczbą, użyj go jako klucza
        key = atoi(argv[1]);
    } else {
        // Jeśli pierwszy argument jest słowem, użyj funkcji haszującej
        for (int i = 0; argv[1][i] != '\0'; i++) {
            key += argv[1][i];  // Suma kodów ASCII znaków
        }
    }
    printf("Wygenerowany klucz kolejki: %d\n", key);

    //Sprawdzanie poprawności Liczby zamówień
    if (atoi(argv[2])==0 || !strcmp(argv[2],"0")) {
        printf("Liczba zamowien powinna byc liczba calkowita dodatania!\n");
        exit(1);
    } else {
        liczba_zamowien = atoi(argv[2]);
        printf("Podano liczbe zamowien: %d \n",liczba_zamowien);
    }

    //Sprawdzanie poprawności max_A
    if (atoi(argv[3])==0 || !strcmp(argv[3],"0")) {
        printf("Argument max_A powinien byc liczba calkowita dodatania\n");
        exit(1);
    } else {
        max_A = atoi(argv[3]);
        printf("Podano max_A: %d \n",max_A);
    }

    //Sprawdzanie poprawności max_B
    if (atoi(argv[4])==0 || !strcmp(argv[4],"0")) {
        printf("Argument max_B powinien byc liczba calkowita dodatania!\n");
        exit(1);
    } else {
        max_B = atoi(argv[4]);
        printf("Podano max_B: %d \n",max_B);
    }

    //Sprawdzanie poprawności max_C
    if (atoi(argv[5])==0 || !strcmp(argv[5],"0")) {
        printf("Argument max_C powinien byc liczba calkowita dodatania!\n");
        exit(1);
    } else {
        max_C = atoi(argv[5]);
        printf("Podano max_C: %d \n",max_C);
    }

    // Tworzenie kolejki komunikatów
    int msgid = msgget(key, 0666 | IPC_CREAT);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }
    printf("Kolejka komunikatów utworzona z ID: %d\n", msgid);

    // Tworzenie kolejki komunikatów
    int msgid_zakonczenie = msgget(key+10, 0666 | IPC_CREAT);
    if (msgid_zakonczenie == -1) {
        perror("msgget");
        exit(1);
    }
    printf("Kolejka komunikatów zamykajaca utworzona z ID: %d\n", msgid_zakonczenie);

    int liczba_aktywnych_magazynow = 0;
    printf("%d - brakujaca liczba aktywnych magazynow do uruchomienia programu\n",3-liczba_aktywnych_magazynow);

    int lista_aktywnych_magazynow[3];

    while(liczba_aktywnych_magazynow < 3){

        struct message zgloszenie_gotowosci;
        if (msgrcv(msgid, &zgloszenie_gotowosci, sizeof(zgloszenie_gotowosci) - sizeof(long), 3, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 3
            if (errno == ENOMSG) {
                sleep(1);
            } else {
                perror("msgrcv");
                exit(1);
            }
        } else {

            struct message dostepnosc_magazynu;
            dostepnosc_magazynu.mtype = 5;
            dostepnosc_magazynu.czy_dostepny_numer_magazynu = 1;

            for (int i = 0; i<liczba_aktywnych_magazynow;i++){
                if (lista_aktywnych_magazynow[i] == zgloszenie_gotowosci.warehouse_id){
                    dostepnosc_magazynu.czy_dostepny_numer_magazynu = 0;
                    break;
                }
            }

            if (msgsnd(msgid, &dostepnosc_magazynu, sizeof(dostepnosc_magazynu) - sizeof(long), 0) == -1) {
                perror("msgsnd dostepnosc_magazynu");
                exit(1);
            }

            if (dostepnosc_magazynu.czy_dostepny_numer_magazynu == 0){
                printf("Magazyn nr.%d juz dziala, uruchom inny magazyn\n",zgloszenie_gotowosci.warehouse_id);
                continue;
            } else{
                lista_aktywnych_magazynow[liczba_aktywnych_magazynow] = zgloszenie_gotowosci.warehouse_id;
            }

            printf("Magazyn nr.%d zglosil gotowosc!\n",zgloszenie_gotowosci.warehouse_id);
            liczba_aktywnych_magazynow++;
            if (liczba_aktywnych_magazynow < 3){
                printf("%d - brakujaca liczba aktywnych magazynow do uruchomienia programu\n",3-liczba_aktywnych_magazynow);
                sleep(1);
            } else{
                printf("Wszystkie magazyny aktywne - program rozpoczyna dzialanie!\n");
            }

        }


    }

    sleep(1);

    int calkowita_ilosc_GLD_do_zaplacenia = 0;

    if (fork()==0){

        while(1){

            struct komunikacja sygnal_wylaczenia_odbior;
            if (msgrcv(msgid_zakonczenie, &sygnal_wylaczenia_odbior, sizeof(sygnal_wylaczenia_odbior) - sizeof(long), 1, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 1
                if (errno == ENOMSG) {
                } else {
                    perror("msgrcv");
                    exit(1);
                }
            } else {
                if(sygnal_wylaczenia_odbior.czy_koniec == 1){
                    printf("Wymuszenie zakonczenia pracy magazynow!\n");
                for(int i = 0; i<9; i++){
                    struct message zakonczenie;
                    zakonczenie.mtype = 4;
                        // Zakończenie procesów kurierów
                    if (msgsnd(msgid, &zakonczenie, sizeof(zakonczenie) - sizeof(long), 0) == -1) {
                        perror("msgsnd zakonczenie");
                        exit(1);
                    }
                }
                    printf("GLD do zaplacenia: %d\n",calkowita_ilosc_GLD_do_zaplacenia);
                    exit(0);
                }
            }
               // Odbieranie odpowiedzi od kurierów
            struct message feedback;
            if (msgrcv(msgid, &feedback, sizeof(feedback) - sizeof(long), 2, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 2
                if (errno == ENOMSG) {
                } else {
                    perror("msgrcv");
                    exit(1);
                }
            } else {
            if (feedback.is_processed == 0){
                liczba_aktywnych_magazynow--;
                if (liczba_aktywnych_magazynow == 0){


                    struct komunikacja sygnal_wylaczenia;
                    sygnal_wylaczenia.mtype = 1;
                    sygnal_wylaczenia.czy_koniec = 1;

                    if (msgsnd(msgid_zakonczenie, &sygnal_wylaczenia, sizeof(sygnal_wylaczenia) - sizeof(long), 0) == -1) {
                        perror("msgid_zakonczenie");
                        exit(1);
                    }
                    printf("Wyslano sygnal zakonczenia\n");

                    sleep(2);

                    printf("GLD do zaplacenia: %d\n",calkowita_ilosc_GLD_do_zaplacenia);

                    exit(0);
                }
            }else{
                printf("zlecenie %d odebrał kurier %d magazynu %d\n",feedback.order_id,feedback.courier_id,feedback.warehouse_id);

                calkowita_ilosc_GLD_do_zaplacenia += feedback.GLD_do_zaplacenia;
            }


            }
        }
    }


    sleep(1);
    if (fork()==0){
        for(int i = 1; i <= liczba_zamowien; i++){

        usleep(500000);

        struct komunikacja sygnal_wylaczenia_odbior;
        if (msgrcv(msgid_zakonczenie, &sygnal_wylaczenia_odbior, sizeof(sygnal_wylaczenia_odbior) - sizeof(long), 1, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 1
            if (errno == ENOMSG) {
            } else {
                perror("msgrcv");
                exit(1);
            }
        } else {
            if(sygnal_wylaczenia_odbior.czy_koniec == 1){
                printf("Magazyny zakonczyly prace\n");
                exit(0);
            }
        }

        // Tworzenie zamówienia
        struct message order;
        order.mtype = 1;  // Typ wiadomości: zamówienie
        order.order_id = i;
        do {
            order.items[0] = losuj_liczbe(max_A);  // Losuj surowiec A
            order.items[1] = losuj_liczbe(max_B);  // Losuj surowiec B
            order.items[2] = losuj_liczbe(max_C);  // Losuj surowiec C
        } while (order.items[0] + order.items[1] + order.items[2] == 0);

        // Wysyłanie zamówienia do kolejki
        if (msgsnd(msgid, &order, sizeof(order) - sizeof(long), 0) == -1) {
            perror("msgsnd");
            exit(1);
        }

        printf("Zlecono zlecenie %d na %dxA, %dxB, %dxC\n", order.order_id,order.items[0],order.items[1],order.items[2]);
        }
        sleep(1);

        struct komunikacja sygnal_wylaczenia;
        sygnal_wylaczenia.mtype = 1;
        sygnal_wylaczenia.czy_koniec = 1;

        if (msgsnd(msgid_zakonczenie, &sygnal_wylaczenia, sizeof(sygnal_wylaczenia) - sizeof(long), 0) == -1) {
            perror("msgid_zakonczenie - for");
            exit(1);
        }
        printf("Wyslano sygnal zakonczenia - for\n");
        exit(0);
    }

    wait(NULL);
    wait(NULL);

    sleep(2);

//     printf("Dystrybutornia musiala zaplacic %d GLD\n",calkowita_ilosc_GLD_do_zaplacenia);

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
    perror("msgctl msgid - usuwanie konalu komunikacyjnego");
    exit(1);
    }
    printf("Kolejka komunikatów usunięta.\n");

    if (msgctl(msgid_zakonczenie, IPC_RMID, NULL) == -1) {
        perror("msgctl msgid_zakonczenie");
        exit(1);
    }
    printf("Kolejka komunikatów zakonczenia usunięta.\n");


    return 0;
}
