#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024

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

// Struktura wiadomości
struct komunikacja {
    long mtype;  // Typ wiadomości
    int items[3];  // Liczba surowców (A, B, C)
    int czy_dostepne; // Status zamówienia (tylko dla odpowiedzi)
    int aktywni_kurierzy;
    int ilosc_GLD;
};

int poprawna_nazwa_pliku(const char *nazwa_pliku) {
    // Sprawdzenie długość nazwy pliku
    if (strlen(nazwa_pliku) != 11) {
        return 0;
    }

    // Sprawdzenie czy nazwa pliku zaczyna się od 'm'
    if (nazwa_pliku[0] != 'm') {
        return 0;
    }

    // Sprawdzenie, czy drugi znak to cyfra od 1 do 10
    char drugi_znak[2] = {nazwa_pliku[1], '\0'};
    if (atoi(drugi_znak)<=0 || atoi(drugi_znak)>10) {
        return 0;
    }


    // Sprawdzenie czy nazwa pliku konczy sie na '_konf.txt'
    const char *koncowka = "_konf.txt";
    if (strcmp(&nazwa_pliku[2], koncowka) != 0) {
        printf("Koncowka\n");
        return 0;
    }

    return 1;
}

int main(int argc, char* argv[]) {

    //Sprawdzam czy jest odpowiednia liczba argumentow
    if (argc != 3){
        printf("Bledna ilosc argumentow. Przy uruchamianiu podaj 2 argumenty (<plik_konfiguracyjny><klucz>)\n");
        exit(1);
    }

    if (poprawna_nazwa_pliku(argv[1])==0){
        printf("Bledna nazwa pliku konfiguracyjnego, poprawna powinna wygladac w nastepujacy sposob: m[numer_magazynu]_konf.txt\n");
        exit(1);
    }


    char znak_magazynu[2] = {argv[1][1], '\0'};
    int numer_magazynu = atoi(znak_magazynu);
    printf("Numer magazynu: %d\n",numer_magazynu);

    int key = 0;
    if (atoi(argv[2])!=0) {
        // Jeśli pierwszy argument jest liczbą, użyj go jako klucza
        key = atoi(argv[2]);
    } else {
        // Jeśli pierwszy argument jest słowem, użyj funkcji haszującej
        for (int i = 0; argv[2][i] != '\0'; i++) {
            key += argv[2][i];  // Suma kodów ASCII znaków
        }
    }



    int zapas_A = 0, zapas_B = 0, zapas_C = 0;
    int cena_A = 0, cena_B = 0, cena_C = 0;

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Nie można otworzyć pliku");
        exit(1);
    }

    char buffer[BUFFER_SIZE];
    char number_buffer[32]; // Bufor na znaki liczby
    int number_index = 0;
    int line_number = 0; // Numer linii (0 - zapasy, 1 - ceny)

    int bytes_read;
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            char ch = buffer[i];

            // Jeśli znak to cyfra, dodaj do bufora liczby
            if (ch >= '0' && ch <= '9') {
                number_buffer[number_index++] = ch;
            }
            // Jeśli znak to spacja lub koniec linii, parsuj liczbę
            else if (ch == ' ' || ch == '\n') {
                if (number_index > 0) { // Jeśli bufor liczby nie jest pusty
                    number_buffer[number_index] = '\0';
                    int number = atoi(number_buffer);
                    number_index = 0; // Resetuj indeks bufora liczby

                    // Przypisz liczbę do odpowiedniej zmiennej
                    if (line_number == 0) { // Pierwsza linia (zapasy)
                        if (zapas_A == 0) zapas_A = number;
                        else if (zapas_B == 0) zapas_B = number;
                        else if (zapas_C == 0) zapas_C = number;
                    } else if (line_number == 1) { // Druga linia (ceny)
                        if (cena_A == 0) cena_A = number;
                        else if (cena_B == 0) cena_B = number;
                        else if (cena_C == 0) cena_C = number;
                    }
                }

                // Jeśli znak to koniec linii, przejdź do następnej linii
                if (ch == '\n') {
                    line_number++;
                }
            }
            // Jeśli znak nie jest cyfrą, spacją ani końcem linii, zgłoś błąd
            else {
                fprintf(stderr, "Błąd: Nieprawidłowy znak w pliku: '%c'\n", ch);
                close(fd);
                exit(1);
            }
        }
    }

    if (bytes_read == -1) {
        perror("Błąd podczas odczytu pliku");
        close(fd);
        exit(1);
    }

    close(fd);

    // Sprawdź, czy wszystkie wartości zostały poprawnie odczytane
    if (zapas_A == 0 || zapas_B == 0 || zapas_C == 0 || cena_A == 0 || cena_B == 0 || cena_C == 0) {
        fprintf(stderr, "Błąd: Niekompletne dane w pliku.\n");
        exit(1);
    }

    // Wypisz odczytane wartości
    printf("Zapasy: A=%d, B=%d, C=%d\n", zapas_A, zapas_B, zapas_C);
    printf("Ceny: A=%d, B=%d, C=%d\n", cena_A, cena_B, cena_C);




    printf("Wygenerowany klucz kolejki: %d\n", key);
    // Kolejka do komunikacji z dystrybutornia
    int msgid = msgget(key, 0666);
    if (msgid == -1) {
        printf("Zeby uruchomic magazyn, najpierw uruchom dyspozytornie!\n");
        exit(1);
    }

    struct message zgloszenie_gotowosci;
    zgloszenie_gotowosci.mtype = 3;
    zgloszenie_gotowosci.warehouse_id = numer_magazynu;

    if (msgsnd(msgid, &zgloszenie_gotowosci, sizeof(zgloszenie_gotowosci) - sizeof(long), 0) == -1) {
        perror("msgid - zgloszenie_gotowosci");
        exit(1);
    }

    struct message dostepnosc_magazynu;
    if (msgrcv(msgid,&dostepnosc_magazynu, sizeof(dostepnosc_magazynu) - sizeof(long), 5, 0) == -1){
        perror("msgrcv - dostepnosc magazynu");
        exit(1);
    }

    if (dostepnosc_magazynu.czy_dostepny_numer_magazynu == 0){
        printf("Magazyn nr.%d juz pracuje, uruchom inny magazyn!\n",numer_magazynu);
        exit(1);
    }






    // Kolejka do komunikacji z kurierami:


    int klucz_2 = key+numer_magazynu;
    int msgid_magazyn = msgget(klucz_2, 0666 | IPC_CREAT);
    if (msgid_magazyn == -1) {
        perror("msgget");
        exit(1);
    }
    printf("Kolejka komunikatów magazyn-kurier utworzona z ID: %d\n", msgid_magazyn);


    int zarobiony_GLD = 0;

    // Komunikacja magazyn - kurierzy
    if (fork()==0){
        int licz_kurierow = 3;
        while(1){
            struct komunikacja zamowienie_magazyn;
            if (msgrcv(msgid_magazyn, &zamowienie_magazyn, sizeof(zamowienie_magazyn) - sizeof(long), 1, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 1
                       if (errno == ENOMSG) {
                    } else {
                        perror("msgrcv - odebranie zamowienia przez magazyn");
                        exit(1);
                    }
                } else {
//             printf("A: %d, B: %d, C: %d\n",zamowienie_magazyn.items[0],zamowienie_magazyn.items[1],zamowienie_magazyn.items[2]);
            if (zamowienie_magazyn.items[0] <= zapas_A && zamowienie_magazyn.items[1] <= zapas_B && zamowienie_magazyn.items[2] <= zapas_C){
//                 printf("Warunek spelniony\n");
                zapas_A -= zamowienie_magazyn.items[0];
                zapas_B -= zamowienie_magazyn.items[1];
                zapas_C -= zamowienie_magazyn.items[2];
                int cena_GLD = zamowienie_magazyn.items[0]*cena_A + zamowienie_magazyn.items[1]*cena_B + zamowienie_magazyn.items[2]*cena_C;

                zarobiony_GLD += cena_GLD;

                struct komunikacja zamowienie_magazyn_odpowiedz;
                zamowienie_magazyn_odpowiedz.mtype = 2;
                zamowienie_magazyn_odpowiedz.czy_dostepne = 1;
                zamowienie_magazyn_odpowiedz.ilosc_GLD = cena_GLD;

                if (msgsnd(msgid_magazyn, &zamowienie_magazyn_odpowiedz, sizeof(zamowienie_magazyn_odpowiedz) - sizeof(long), 0) == -1) {
                        perror("msgid_magazyn - odpowiedz");
                        exit(1);
                    }
            } else {
                printf("Warunek niespelniony\n");
                licz_kurierow--;
                struct komunikacja zamowienie_magazyn_odpowiedz;
                zamowienie_magazyn_odpowiedz.mtype = 2;
                zamowienie_magazyn_odpowiedz.czy_dostepne = 0;
                zamowienie_magazyn_odpowiedz.aktywni_kurierzy = licz_kurierow;

                printf("Stan zapasow: A=%d, B=%d, C=%d\n", zapas_A, zapas_B, zapas_C);

                if (msgsnd(msgid_magazyn, &zamowienie_magazyn_odpowiedz, sizeof(zamowienie_magazyn_odpowiedz) - sizeof(long), 0) == -1) {
                        perror("msgid_magazyn - odpowiedz");
                        exit(1);
                    }
                printf("Nie udalo sie zrealizowac zamowienia!\n");
            }
                }
            struct komunikacja wylaczenie_kuriera_magazyn;
                if (msgrcv(msgid_magazyn, &wylaczenie_kuriera_magazyn, sizeof(wylaczenie_kuriera_magazyn) - sizeof(long), 3, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 3
                    if (errno == ENOMSG) {
                    } else {
                        perror("msgrcv");
                        exit(1);
                    }
                } else {
                    licz_kurierow--;
                    printf("Licznik kurierow: %d\n",licz_kurierow);
                    if (licz_kurierow == 0){
                        sleep(1);
                        printf("Magazyn nr.%d konczy prace zarabiajac: %d GLD\n",numer_magazynu,zarobiony_GLD);
                        exit(0);
                    }
                }

            if (licz_kurierow == 0){
                    printf("Magazyn nr.%d konczy prace zarabiajac: %d GLD\n",numer_magazynu,zarobiony_GLD);
                    exit(0);
                }

        }

    }

    // Inicjalizacja czasu dla kurierow
    time_t last_received_time = time(NULL);

    // Kurierzy
    for (int i = 1; i <=3; i++){
        if (fork()==0){

            int courier_id = i;    // ID kuriera
            int warehouse_id = numer_magazynu;  // ID magazynu
            while (1) {

                struct message zakonczenie;
                if (msgrcv(msgid, &zakonczenie, sizeof(zakonczenie) - sizeof(long), 4, IPC_NOWAIT) == -1) {  // Odbieranie tylko wiadomości typu 4
                    if (errno == ENOMSG) {
                    } else {
                        perror("msgrcv");
                        exit(1);
                    }
                } else {
                    struct komunikacja wylaczenie_kuriera;
                    wylaczenie_kuriera.mtype = 3;

                    if (msgsnd(msgid_magazyn, &wylaczenie_kuriera, sizeof(wylaczenie_kuriera) - sizeof(long), 0) == -1) {
                        perror("msgid_magazyn - wylaczenie_kuriera");
                        exit(1);
                    }

                    printf("Kurier nr.%d wylaczony!\n",courier_id);
                    exit(0);

                }

                struct message order;

                // Odbieranie zamówienia z timeoutem
                if (msgrcv(msgid, &order, sizeof(order) - sizeof(long), 1, IPC_NOWAIT) == -1) {
                    if (errno == ENOMSG) {
                        // Brak wiadomości w kolejce
                        time_t current_time = time(NULL);
                        int max_czas_aktywnosci = 150;
                        if (current_time - last_received_time >= max_czas_aktywnosci) {
                            printf("Kurier %d: Brak wiadomości przez %d sekund. Zamykanie procesu.\n",courier_id,max_czas_aktywnosci);

                            struct komunikacja wylaczenie_kuriera;
                            wylaczenie_kuriera.mtype = 3;

                            if (msgsnd(msgid_magazyn, &wylaczenie_kuriera, sizeof(wylaczenie_kuriera) - sizeof(long), 0) == -1) {
                                perror("msgid_magazyn - wylaczenie_kuriera");
                                exit(1);
                            }

                            printf("Kurier nr.%d wylaczony!\n",courier_id);
                            exit(0);

                        }
                    } else {
                        perror("msgrcv");
                        exit(1);
                    }
                } else {
                    // Zaktualizuj czas ostatnio odebranej wiadomości
                    last_received_time = time(NULL);


                    struct komunikacja zamowienie_magazyn;
                    zamowienie_magazyn.mtype = 1;
                    zamowienie_magazyn.items[0] = order.items[0];
                    zamowienie_magazyn.items[1] = order.items[1];
                    zamowienie_magazyn.items[2] = order.items[2];

                    if (msgsnd(msgid_magazyn, &zamowienie_magazyn, sizeof(zamowienie_magazyn) - sizeof(long), 0) == -1) {
                        perror("msgid_magazyn - zamowienie");
                        exit(1);
                    }

//                     printf("Kurier %d wyslal zamowienie nr %d do magazynu.\n", courier_id, order.order_id);

                    struct komunikacja zamowienie_magazyn_odpowiedz;
                    if (msgrcv(msgid_magazyn, &zamowienie_magazyn_odpowiedz, sizeof(zamowienie_magazyn_odpowiedz) - sizeof(long), 2, 0) == -1) {  // Odbieranie tylko wiadomości typu 2
                        perror("zamowienie_magazyn - odbior kurier");
                        exit(1);
                    }

                    if (zamowienie_magazyn_odpowiedz.czy_dostepne == 1){
//                         printf("Zamowienie dostepne, do zaplacenie %d GLD\n",zamowienie_magazyn_odpowiedz.ilosc_GLD);
                    } else {
                        if (zamowienie_magazyn_odpowiedz.aktywni_kurierzy == 0){
                            struct message feedback;
                            feedback.mtype = 2;
                            feedback.is_processed = 0;
                            if (msgsnd(msgid, &feedback, sizeof(feedback) - sizeof(long), 0) == -1) {
                                perror("msgsnd (feedback)");
                                exit(1);
                            }
                        }

                        printf("Zamowienie niedostepne, kurier konczy prace\n");
                        exit(0);
                    }

                    int is_processed = 1;
                    // Wysyłanie odpowiedzi do dyspozytorni
                    struct message feedback;
                    feedback.mtype = 2;  // Typ wiadomości: odpowiedź
                    feedback.order_id = order.order_id;
                    feedback.courier_id = courier_id;
                    feedback.warehouse_id = warehouse_id;
                    feedback.is_processed = is_processed;
                    feedback.GLD_do_zaplacenia = zamowienie_magazyn_odpowiedz.ilosc_GLD;

                    if (msgsnd(msgid, &feedback, sizeof(feedback) - sizeof(long), 0) == -1) {
                        perror("msgsnd (feedback)");
                        exit(1);
                    }

//                     printf("Kurier %d wyslal informacje zwrotna dla zamowienia: %d\n", courier_id, order.order_id);
                }

    //             // Poczekaj chwilę przed kolejną iteracją, aby nie obciążać CPU
    //             sleep(1);
            }
        }
    }


    // Proces macierzysty czeka na zakończenie procesów potomnych
    for (int i = 0; i < 4; i++) {
        wait(NULL);
    }
    if (msgctl(msgid_magazyn, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
    printf("Kolejka komunikatów magazyn - kurierzy usunięta.\n");

    return 0;
}
