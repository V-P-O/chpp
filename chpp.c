// standardni c library
#include<stdlib.h>
#include<stdio.h>
#include<math.h>
#include<string.h>
#include<stdbool.h>
#include<stdint.h>
#include<wchar.h> // koristim wide char da pohranim unicode charove
#include<time.h> // jedina svrha je radnom seed

// windowsovi library koristeni za ispis i ulaz unicode znakova
#include <fcntl.h>
#include <io.h>

#ifndef _O_U16TEXT
  #define _O_U16TEXT 0x20000
#endif

double str_to_num(wchar_t * str){ // pretvara wchar_t* u double npr "2.5" u 2.5
    int len = wcslen(str);
    double ret = 0;
    int i,k;
    int i2 = 0;
    bool t = false;
    double factor = 0.5;
    if (str[0] == L'-'){
        i2 = 1;
        factor = -0.5;
    }
    for (i = i2; i<len; i++){

        if (str[i] == L'.'){
            ret = ret / pow(10, (len+(factor-0.5))-(i-i2));
            t = true;
            k = 1;
        }
        else if (!t) ret = ret + (str[i]-48) * pow(10, (len+(factor-0.5))-(i-i2)-1);
        else if (t) {ret = ret + (str[i]-48) / pow(10, k); k++;}
    }
    return ret*(factor*2);
}

typedef struct{  // struktura dinamicne liste koristene u cvorovima
    int * el;    // array integera
    int size;    // broj elemenata/ brojeva
    int cap;     // velicina el pointera
}list;

void init(list * List, int el_num){  // alocira memoriju za listu

    List->cap = el_num;
    List->el = calloc(el_num, sizeof(int));
    List->size = 0;

}

void append(list * List, int element){ // dodava element listi

    if (List->size < List->cap){
        List->el[List->size] = element;
        List->size++;
    }
    if (List->size == List->cap){
        List->el = realloc(List->el , List->cap*sizeof(int)*2);
        List->cap = List->cap *2;
    }

}


unsigned int ht_size = 10007;   // velicina hashtable-a u kojem su pohranjene varijable
int current_size = 0;           // trenutna velicina hashtable-a
int ht_offset = 0;
int ht_const = 3;
int global_size = 0;            // broj globalnih varijabla

bool interpreter_error = false;     // trua ako je dopslo do errora u interpreteru

typedef struct{     // varijabla i svi podaci u jeziku
    uint8_t type;      // tip podatka,  35 - broj, 34 - niz, 17 - funkcija, 23 - lista, 0 - nista
    bool fre;       // ako true data pointer se moze osloboditi
    void * data;    // podatke koje varijabla sadrzi
    int size;       // velicina varijable
    int cap;        // velicina data*
}variable;

typedef struct{     // elementi hashtable-a
    variable var;       // varijabla koju sadrzi
    wchar_t * key;      // kljuc
    bool global;        // jeli varijabla globalna
}entry;

entry ** hashtable;  // declare hashtable

int * var_stack;        // stack koji sadrzi indekse hashtable-a
int stack_size = 0;     // velicina stack-a

void ffree(variable * file){        // oslobada memoriju iz varijable koja je tipa datoteka
    fclose ( ((FILE**)file->data)[0] );
    free (file->data);
}

void freelist(variable * lst, int index){   // oslobada memoriju iz varijable koji je tipa lista
    int sz = lst->size;
    int i;
    for (i = 0; i<sz; i++){
        if (i != index){
            variable temp = ((variable*)lst->data)[i];
            switch (temp.type){
                case 16 :
                    ffree(&temp);
                case 23 :
                    freelist(&temp,-1);
                break;
                case 17 :
                case 34 :
                case 35 :
                    free(temp.data);
                break;
            }
        }
    }
    free(lst->data);
}

void ht_setup(){        // postavi svaki element hashtable-a na NULL
    int i;
    for (i = 0; i<ht_size; i++){
        hashtable[i] = NULL;
    }
}

unsigned long int hash(wchar_t * Key){      // vraca indeks hashtable-a za dani kljuc
    unsigned long int value = 0;
    int i = 0;
    while (Key[i] != L'\0'){
        value = value * 37 + Key[i];
        i++;
    }
    if (!( (current_size * ht_const) + (ht_offset * ht_const) ) ) return 0;
    return value % (current_size * ht_const) + (ht_offset * ht_const);
}

unsigned long int is_global(wchar_t * Key){     // provjeri je li varijabla s tim kljucem globalna

    unsigned long int value = 0;
    int i = 0;
    while (Key[i] != L'\0'){
        value = value * 37 + Key[i];
        i++;
    }
    if (!( (global_size * ht_const) )) return 0;
    value = value % (global_size * ht_const);

    while (hashtable[value] != NULL){
        if (wcscmp(hashtable[value]->key, Key) == 0){
            if ( hashtable[value]->global ) return value;
            else return -1;
        }
        value++;
        value = value % (global_size * ht_const);
    }
    return -1;


}

void ht_put(wchar_t * Key, variable Var, int global){       // stavlja novi entry u hashtable ili mijenja varijablu na nekom entry-u

    unsigned long int value;

    if (ht_offset){

        value = is_global(Key);
        if(value == -1) value = hash(Key);
    }
    else value = hash(Key);

    while (true){

        if (hashtable[value] == NULL) {
            hashtable[value] = malloc(sizeof(entry));
            hashtable[value]->key = malloc(sizeof(wchar_t) * (wcslen(Key) + 1) );
            wcscpy(hashtable[value]->key, Key);
            hashtable[value]->var = Var;
            hashtable[value]->global = !global;

            var_stack[stack_size] = value;
            stack_size++;
            break;
        }
        else if (wcscmp(hashtable[value]->key, Key) == 0){
            if (hashtable[value]->var.type == 23)freelist(&hashtable[value]->var,-1);
            else free(hashtable[value]->var.data);
            hashtable[value]->var = Var;
            break;
        }

        value++;
        if (value >= (current_size * ht_const) + (ht_offset * ht_const) )value = ht_offset * ht_const;
    }

}
variable ht_read(wchar_t * Key){        // vraca varijablu koja je u entry-u s danim kljucem

    unsigned long int value;

    if (ht_offset){

        value = is_global(Key);
        if(value == -1) value = hash(Key);
    }
    else value = hash(Key);

    while (hashtable[value] != NULL){
        if (wcscmp(hashtable[value]->key, Key) == 0){
            return hashtable[value]->var;
        }
        value++;
        if (value >= (current_size * ht_const) + (ht_offset * ht_const) )value = ht_offset * ht_const;
    }
    variable nulvar;
    nulvar.type = 68;
    nulvar.cap = 1; nulvar.size = 1;
    return nulvar;

}
entry * ht_read2(wchar_t * Key){        // vraca pointer na entry koji sadrzi dani kljuc

    unsigned long int value;

    if (ht_offset){

        value = is_global(Key);
        if(value == -1) value = hash(Key);
    }
    else value = hash(Key);

    while (hashtable[value] != NULL){
        if (wcscmp(hashtable[value]->key, Key) == 0){
            return hashtable[value];
        }
        value++;
        if (value >= (current_size * ht_const) + (ht_offset * ht_const) )value = ht_offset * ht_const;
    }

    entry * nulep = NULL;
    return nulep;

}

typedef struct{     // struktura token
    uint8_t type;          // kojeg je tipa
    wchar_t * data;
    int line;           // na kojoj je liniji u tekstu koda
}token;

typedef struct{     // node tj. cvor
    token tok;      // token kojeg cvor predstavlja
    list kids;      // djeca cvora
    int parent;     // roditelj cvora
    int id;         // indeks u array-u cvorova
}node;

int toknum = 0;     // broj tokena
int tokenptr = 0;   // indeks tokena kojeg parser zahtjeva
int nodesptr = 0;   // broj cvorova
token * tokens;     // array tokena
node * nodes;       // array cvorova

bool parser_error = false;      // true ako se dogodi error u parseru

node parse_0();         // deklaracija funkcija parsera
node parse_1();         // ima ih 10 jer koristim recursive descent parser algoritam
node parse_2();
node parse_3();
node parse_4();
node parse_5();
node parse_6();
node parse_7();
node parse_8();
node parse_9();

wchar_t * type_to_str(int type){    // argument je broj koji oznacava tip podatka, vraca ime tipa podatka u obliku whar pointera
    wchar_t * ret;
    ret = malloc(20);
    switch (type){
        case 0 : wcscpy(ret, L"ništa"); return ret;
        case 16 : wcscpy(ret, L"datoteka"); return ret;
        case 17 : wcscpy(ret, L"funkcija"); return ret;
        case 23 : wcscpy(ret, L"lista"); return ret;
        case 34 : wcscpy(ret, L"niz"); return ret;
        case 35 : wcscpy(ret, L"broj"); return ret;
    }
}

void printlist(variable * lst){     // ispisuje podatke u varijabli tipa lista
    int sz = lst->size;
    int i;
    wprintf(L"[");
    for (i = 0; i<sz; i++){
        variable temp = ((variable*)lst->data)[i];
        if (i != 0) wprintf(L", ");
        switch (temp.type){
            case 17 :
                if (*((int*)temp.data) > 0) wprintf(L"Funkcija na liniji %d", nodes[ *((int*)temp.data) ].tok.line );
                else if (*((int*)temp.data) < 0) wprintf(L"Ugrađena funkcija");
            break;
            case 23 :
                printlist(&temp);
            break;
            case 34 :
                wprintf(L"\"%ls\"", (wchar_t*)temp.data );
            break;
            case 35 :
                if (true){
                    double number = *((double*)temp.data);
                    wchar_t * buffer = calloc(100, sizeof(wchar_t));
                    swprintf(buffer, 70 ,L"%.15f", number);
                    int nullpos=49;
                    while (buffer[nullpos] == 0 || buffer[nullpos] == 48) nullpos--;
                    if (buffer[nullpos] == L'.') nullpos--;
                    buffer[nullpos+1] = L'\0';
                    wprintf(L"%ls", buffer );
                    free(buffer);
                }
            break;
            default :
                wprintf(L"ništa");
            break;
        }
    }
    wprintf(L"]");
}

bool comparelist(variable * lst1, variable * lst2){     // usporeduje dvije varijable tipa lista
    int sz = lst1->size;
    if (lst1->size != lst2->size) return false;
    int i;
    for (i = 0; i<sz; i++){
        variable temp1 = ((variable*)lst1->data)[i];
        variable temp2 = ((variable*)lst2->data)[i];
        if (temp1.type != temp2.type) return false;
        switch (temp1.type){
            case 16 :
                if (memcmp(temp1.data, temp2.data, 9))return false;
            break;
            case 17 :
                if ( *((int*)temp1.data) != *((int*)temp2.data) ) return false;
            break;
            case 23 :
                if (!comparelist(&temp1, &temp2)) return false;
            break;
            case 34 :
                if (wcscmp(temp1.data, temp2.data))return false;
            break;
            case 35 :
                if ( *((double*)temp1.data) != *((double*)temp2.data) ) return false;
            break;
        }
    }
    return true;
}

variable copylist(variable lst){        // kopira varijablu tipa lista
    int sz = lst.size;
    int i;
    variable ret;
    ret.size = lst.size;
    ret.cap = lst.size;
    ret.type = lst.type;
    ret.fre = true;
    ret.data = malloc(sizeof(variable) * ret.size);
    for (i = 0; i<sz; i++){

        variable a = ((variable*)lst.data)[i];
        if (a.type == 23)a = copylist(a);
        variable temp;
        temp.cap = a.size;
        temp.size = a.size;
        temp.type = a.type;
        temp.fre = true;

        switch (temp.type){
            case 0 :
                temp.fre = false;
            break;
            case 16 :
                temp.data = malloc( 9 );
                memcpy(temp.data, a.data, 9 );
            break;
            case 17 :
                temp.data = malloc(sizeof(int) );
                memcpy(temp.data, a.data, sizeof(int) );
            break;
            case 23 :
                temp.data = malloc(sizeof(variable) * (a.size) );
                memcpy(temp.data, a.data, sizeof(variable) * (a.size) );
            break;
            case 34 :
                temp.data = malloc(sizeof(wchar_t) * (a.size+1) );
                memcpy(temp.data, a.data, sizeof(wchar_t) * (a.size+1) );
            break;
            case 35 :
                temp.data = malloc(sizeof(double));
                memcpy(temp.data, a.data, sizeof(double) );
            break;
        }
        memcpy(ret.data+i*sizeof(variable), &temp, sizeof(variable));
    }
    return ret;
}

variable interpret(node Node){      // funkcija rekurzivno ide po apstraktnom sintaksnom stablu

    variable ret;       // variabla koju ce funkcija vracati
    ret.type = 0;
    ret.size = 0; ret.cap = 0;
    ret.fre = true;
    ret.data = NULL;
    int i;
    switch (Node.tok.type){     // oviseci o tipu tokena kojeg cvor predstavlja funkcija izvrsava drukcije naredbe
        case 25 :       // broj koji oznacava viticastu zagradu, izvrsava blok č++ koda
            for (i = 0; i<Node.kids.size; i++){
                ret = interpret( nodes[Node.kids.el[i]] );
                if (interpreter_error){
                    ret.type = 0;
                    ret.cap = 0;ret.size = 0;
                    ret.fre = false;
                    return ret;
                }
                if (ret.type >= 120){
                    ret.type = ret.type - 120;
                    return ret;
                }
                if (ret.fre)free(ret.data);
            }
            ret.type = 0;
            ret.cap = 0;ret.size = 0;
            ret.fre = false;
            return ret;
        break;
        case 1 :        // jednako, usporeduje djecu
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) == *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = !wcscmp(var1.data, var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 23 && var2.type == 23){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 0;
                    if ( comparelist( &var1, &var2) ) temp = 1;
                    if (var1.fre)freelist(&var1, -1);
                    if (var2.fre)freelist(&var2, -1);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 17 && var2.type == 17){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((int*)var1.data) == *((int*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 16 && var2.type == 16){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = !memcmp(var1.data, var2.data, 9);
                    if (var1.fre)ffree(&var1);
                    if (var2.fre)ffree(&var2);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 0 ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
            }
        break;
        case 2 :    // ne jednako, usporeduje djecu
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) != *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = !!wcscmp(var1.data, var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 23 && var2.type == 23){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 1;
                    if ( comparelist( &var1, &var2) ) temp = 0;
                    if (var1.fre)freelist(&var1, -1);
                    if (var2.fre)freelist(&var2, -1);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 17 && var2.type == 17){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((int*)var1.data) != *((int*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 16 && var2.type == 16){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = !!memcmp(var1.data, var2.data, 9);
                    if (var1.fre)ffree(&var1);
                    if (var2.fre)ffree(&var2);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 1 ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
            }
        break;
        case 3 :       // vece ili jednako, usporeduje djecu
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) >= *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 1;
                    i = 0;
                    while (1){
                        if (((wchar_t*)var1.data)[i] > ((wchar_t*)var2.data)[i]){
                            temp = 1;
                            break;
                        }
                        if (((wchar_t*)var1.data)[i] < ((wchar_t*)var2.data)[i]){
                            temp = 0;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] == L'\0' && ((wchar_t*)var2.data)[i] == L'\0'){ temp=1 ; break; }
                        i++;
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator >= \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 4 :        // manje ili jednako, usporeduje djecu
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) <= *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 0;
                    i = 0;
                    while (1){
                        if (((wchar_t*)var1.data)[i] < ((wchar_t*)var2.data)[i]){
                            temp = 1;
                            break;
                        }
                        if (((wchar_t*)var1.data)[i] > ((wchar_t*)var2.data)[i]){
                            temp = 0;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] == L'\0' && ((wchar_t*)var2.data)[i] == L'\0'){ temp=1 ; break; }
                        i++;
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator <= \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 5 :        // cjelobrojno djeljenje djece
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = trunc ( *((double*)var1.data) / *((double*)var2.data) );
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator // \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 6 :        // operator and
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = ( (*((double*)var1.data)!=0) + (*((double*)var2.data)!=0) ) == 2;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator I \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 8 :        // operator not
            if (Node.kids.size == 1){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;

                if (var1.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = (*((double*)var1.data)==0) ;
                    if (var1.fre)free(var1.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Nevaljani operand %ls za operator NE \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 9 :        //  for loop

            if (Node.kids.size>3){

                variable temp = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (temp.fre)free(temp.data);
                variable uvjet;
                bool brk = false;

                while (1){

                    uvjet = interpret( nodes[Node.kids.el[1]] );
                    if (interpreter_error) return ret;

                    if (uvjet.type == 35){

                        if ( *((double*)uvjet.data) != 0 ){

                            for (i = 3; i<Node.kids.size; i++){

                                ret = interpret( nodes[Node.kids.el[i]] );
                                if (interpreter_error) return ret;
                                if (ret.type == 38){
                                    brk = true;
                                    break;
                                }
                                if (ret.type >= 120){
                                    return ret;
                                }
                                if (ret.fre)free(ret.data);
                            }
                            if (brk) break;
                            variable temp = interpret( nodes[Node.kids.el[2]] );
                            if (interpreter_error) return ret;
                            if (temp.fre)free(temp.data);
                        }
                        else{ if (uvjet.fre)free(uvjet.data); break;}
                    }
                    else{ if (uvjet.fre)free(uvjet.data); break;}

                }
                ret.type = 0;
                ret.fre = false;
                return ret;
            }

        break;
        case 10 :       //  operator or
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = ( (*((double*)var1.data)!=0) + (*((double*)var2.data)!=0) ) >= 1;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator ILI\n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 11 :       // if

            if (Node.kids.size>0){

                variable uvjet = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                int elsebool = nodes[Node.kids.el[Node.kids.size-1]].tok.type == 15;

                if (uvjet.type == 35){

                    if ( *((double*)uvjet.data) != 0 ){

                        for (i = 1; i<Node.kids.size - elsebool; i++){

                            ret = interpret( nodes[Node.kids.el[i]] );
                            if (interpreter_error) return ret;
                            if (ret.type == 38){
                                return ret;
                            }
                            if (ret.type >= 120){
                                return ret;
                            }
                            if (ret.fre)free(ret.data);
                        }
                    }
                    else if ( elsebool ){
                        variable ret = interpret( nodes[Node.kids.el[Node.kids.size-1]] );
                        if (interpreter_error) return ret;
                        if (ret.type == 38){
                            return ret;
                        }
                        if (ret.type >= 120){
                            return ret;
                        }
                        if (ret.fre)free(ret.data);
                    }
                }
                if (uvjet.fre)free(uvjet.data);
                ret.type = 0;
                ret.fre = false;
                return ret;
            }

        break;
        case 13 :       //  while loop

            if (Node.kids.size>1){

                variable uvjet;
                bool brk = false;

                while (1){

                    uvjet = interpret( nodes[Node.kids.el[0]] );
                    if (interpreter_error) return ret;

                    if (uvjet.type == 35){

                        if ( *((double*)uvjet.data) != 0 ){

                            for (i = 1; i<Node.kids.size; i++){

                                ret = interpret( nodes[Node.kids.el[i]] );
                                if (interpreter_error) return ret;
                                if (ret.type == 38){
                                    brk = true;
                                    break;
                                }
                                if (ret.type >= 120){
                                    return ret;
                                }
                                if (ret.fre)free(ret.data);
                            }
                            if (brk)break;
                        }
                        else{ if (uvjet.fre)free(uvjet.data); break;}
                    }
                    else{ if (uvjet.fre)free(uvjet.data); break;}

                }
                ret.type = 0;
                ret.fre = false;
                return ret;
            }

        break;
        case 14 :       // return
            if (Node.kids.size == 1){
                ret = interpret(nodes[Node.kids.el[0]]);
                if (interpreter_error) return ret;
                if (!ret.fre){
                    variable temp;
                    temp.cap = ret.cap; temp.size = ret.size;
                    temp.type = ret.type;
                    switch (ret.type){
                        case 17 :
                            temp.data = malloc(sizeof(int));
                            memcpy(temp.data, ret.data, sizeof(int) );
                        break;
                        case 23 :
                            temp = copylist(ret);
                        break;
                        case 34 :
                            temp.data = malloc(sizeof(wchar_t) * (temp.size+1) );
                            memcpy(temp.data, ret.data, sizeof(wchar_t) * (temp.size+1) );
                        break;
                        case 35 :
                            temp.data = malloc(sizeof(double));
                            memcpy(temp.data, ret.data, sizeof(double) );
                        break;
                    }
                    temp.type = temp.type + 120;
                    return temp;
                }
                ret.type = ret.type + 120;
                return ret;
            }
        break;
        case 15 :       // else

            if (Node.kids.size>0){

                for (i = 0; i<Node.kids.size ; i++){
                    ret = interpret( nodes[Node.kids.el[i]] );
                    if (interpreter_error) return ret;
                    if (ret.type == 38){
                        return ret;
                    }
                    if (ret.type >= 120){
                        return ret;
                    }
                    if (ret.fre)free(ret.data);
                }
                ret.type = 0;
                ret.fre = false;
                return ret;
            }

        break;
        case 17 :       // funkcija
            if (Node.kids.size > 1){
                variable func;
                func.fre = false;
                func.cap = 1;
                func.size = 1;
                func.type = 17;
                func.data = malloc(sizeof(int));
                int temp = Node.id;
                memcpy(func.data, &temp, sizeof(int));
                ht_put( nodes[Node.kids.el[0]].tok.data , func , Node.parent);
                ret.fre = false; ret.type = 0;
                return ret;
            }
        break;
        case 18 :       // vise od
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) > *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 0;
                    i = 0;
                    while (1){
                        if (((wchar_t*)var1.data)[i] > ((wchar_t*)var2.data)[i]){
                            temp = 1;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] < ((wchar_t*)var2.data)[i]){
                            temp = 0;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] == L'\0' || ((wchar_t*)var2.data)[i] == L'\0')break;
                        i++;
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator > \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 19 :       //  manje od
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) < *((double*)var2.data) ;
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = 0;
                    i = 0;
                    while (1){
                        if (((wchar_t*)var1.data)[i] < ((wchar_t*)var2.data)[i]){
                            temp = 1;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] > ((wchar_t*)var2.data)[i]){
                            temp = 0;
                            break;
                        }
                        else if (((wchar_t*)var1.data)[i] == L'\0' || ((wchar_t*)var2.data)[i] == L'\0')break;
                        i++;
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator < \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 20 :       // asign
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;
                if (nodes[Node.kids.el[0]].tok.type == 36 && nodes[Node.kids.el[0]].kids.size == 0 ){

                    if (var1.fre) ht_put( nodes[Node.kids.el[0]].tok.data , var1 , Node.parent);
                    else {
                        variable temp;
                        temp.cap = var1.cap; temp.size = var1.size;
                        temp.type = var1.type;
                        switch (var1.type){
                            case 0 :
                                temp.data = NULL;
                            break;
                            case 16 :
                                temp.data = malloc(9);
                                memcpy(temp.data, var1.data, 9 );
                            break;
                            case 17 :
                                temp.data = malloc(sizeof(int));
                                memcpy(temp.data, var1.data, sizeof(int) );
                            break;
                            case 23 :
                                temp = copylist(var1);
                            break;
                            case 34 :
                                temp.data = malloc(sizeof(wchar_t) * (var1.size+1) );
                                memcpy(temp.data, var1.data, sizeof(wchar_t) * (var1.size+1) );
                            break;
                            case 35 :
                                temp.data = malloc(sizeof(double));
                                memcpy(temp.data, var1.data, sizeof(double) );
                            break;
                        }
                        ht_put( nodes[Node.kids.el[0]].tok.data , temp , Node.parent);
                    }

                }
                else if (nodes[Node.kids.el[0]].tok.type == 37){
                    nodes[Node.kids.el[0]].tok.data[0] = 20;
                    nodes[Node.kids.el[0]].kids.cap = 1;
                    variable indexes = interpret(nodes[Node.kids.el[0]]);
                    if (interpreter_error) return ret;
                    entry * varptr = ht_read2(nodes[indexes.size].tok.data);
                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[indexes.size].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    if (varptr->var.type != 23){
                        wchar_t * op1 = type_to_str(varptr->var.type);
                        wprintf(L"Linija %d : Tip podatka %ls ne podržava dodjeljivanje elementu\n", Node.tok.line, op1);
                        free(op1);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    variable * temp = ((variable*)varptr->var.data);
                    int index, list_size;
                    uint8_t temp_type = 23;
                    list_size = varptr->var.size;
                    for (i = 0; i < indexes.cap-1; i++){
                        index = ((int*)indexes.data)[i];
                        if (index >= list_size || index < 0){
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                            return ret;
                        }
                        temp_type = temp[index].type;
                        list_size = temp[index].size;
                        if (temp_type == 23) temp =  (variable *)(temp[index].data);
                        else{
                            wchar_t * op1 = type_to_str(temp_type);
                            wprintf(L"Linija %d : Tip podatka %ls ne podržava dodjeljivanje elementu\n", Node.tok.line, op1);
                            free(op1);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }

                    index = ((int*)indexes.data)[indexes.cap-1];
                    if (index >= list_size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                        return ret;
                    }

                    if (var1.fre){
                        if (temp[ index ].type == 23) freelist( &(temp[ index ]), -1 );
                        else free( temp[ index ].data );
                        temp[ index ] = var1;
                    }
                    else {
                        variable temp2;
                        temp2.cap = var1.cap; temp2.size = var1.size;
                        temp2.type = var1.type;
                        switch (var1.type){
                            case 23 :
                                temp2 = copylist(var1);
                            break;
                            case 34 :
                                temp2.data = malloc(sizeof(wchar_t) * (var1.size+1) );
                                memcpy(temp2.data, var1.data, sizeof(wchar_t) * (var1.size+1) );
                            break;
                            case 35 :
                                temp2.data = malloc(sizeof(double));
                                memcpy(temp2.data, var1.data, sizeof(double) );
                            break;
                        }
                        if (temp[ index ].type == 23) freelist( &(temp[ index ]), -1 );
                        else free( temp[ index ].data );
                        temp[ index ] = temp2;
                    }

                }
            }
            ret.type = 0;
            ret.fre = false;
            return ret;
        break;
        case 21 :       // pozovi funkciju
            if (Node.kids.size >= 2){
                variable func = interpret( nodes[Node.kids.el[Node.kids.size-1]] );
                if (interpreter_error){
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if (func.type != 17){
                    wchar_t * op1 = type_to_str(func.type);
                    wprintf(L"Linija %d : Tip podatka %ls ne može biti pozvan \n", Node.tok.line, op1);
                    free(op1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                int ndnum = *((int*)func.data);

                if (ndnum < 0){
                    node temp_node;
                    temp_node.kids = Node.kids;
                    temp_node.kids.size--;
                    temp_node.tok.line = Node.tok.line;
                    temp_node.tok.type = -ndnum;
                    temp_node.id = 0; temp_node.parent = 0;
                    ret = interpret(temp_node);
                    return ret;
                }

                if (Node.kids.size-1 != nodes[ndnum].kids.size-2 && !(Node.kids.size == 2 && nodes[Node.kids.el[0]].tok.type == 22) ){
                    wchar_t trz[10];
                    wchar_t dati[10];
                    wchar_t arg[10];
                    if (nodes[ndnum].kids.size-2 == 1) wcsncpy(trz, L"Tražen", 14);
                    else wcsncpy(trz, L"Traženo", 16);
                    if (nodes[ndnum].kids.size-2 == 1) wcsncpy(arg, L"argument", 18);
                    else wcsncpy(arg, L"argumenta", 20);
                    if (Node.kids.size-1 == 1) wcsncpy(dati, L"dan", 8);
                    else wcsncpy(dati, L"dano", 10);
                    wprintf(L"Linija %d : %ls %d %ls, a %ls je %d \n", Node.tok.line, trz, nodes[ndnum].kids.size-2, arg, dati, Node.kids.size-1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

                variable * params;
                if ( !(Node.kids.size == 2 && nodes[Node.kids.el[0]].tok.type == 22) ){
                    params = malloc( (Node.kids.size - 1) * sizeof(variable));
                    for (i = 0; i<Node.kids.size-1; i++){
                        params[i] = interpret( nodes[Node.kids.el[i]] );
                    }
                }

                int prev_size = current_size;
                int cur_stack_size = stack_size;
                ht_offset += current_size;
                current_size = *((int*)nodes[ndnum].tok.data);

                variable func_temp;
                func_temp.cap = func.cap; func_temp.type = 17;
                func_temp.data = malloc(sizeof(int));
                memcpy(func_temp.data, func.data, sizeof(int));
                ht_put( nodes[nodes[ndnum].kids.el[0]].tok.data , func_temp , 1);


                for (i = 1; i<nodes[ndnum].kids.size-1; i++){

                    if (nodes[nodes[ndnum].kids.el[i]].tok.type == 36){

                        if (params[i-1].fre) ht_put( nodes[nodes[ndnum].kids.el[i]].tok.data , params[i-1] , 10 );
                        else {
                            variable temp;
                            temp.cap = params[i-1].cap; temp.size = params[i-1].size;
                            temp.type = params[i-1].type;
                            switch (params[i-1].type){
                                case 17 :
                                    temp.data = malloc(sizeof(int));
                                    memcpy(temp.data, params[i-1].data, sizeof(int) );
                                break;
                                case 23 :
                                    temp = copylist(params[i-1]);
                                break;
                                case 34 :
                                    temp.data = malloc(sizeof(wchar_t) * (temp.size+1) );
                                    memcpy(temp.data, params[i-1].data, sizeof(wchar_t) * (temp.size+1) );
                                break;
                                case 35 :
                                    temp.data = malloc(sizeof(double));
                                    memcpy(temp.data, params[i-1].data, sizeof(double) );
                                break;
                            }
                            ht_put( nodes[nodes[ndnum].kids.el[i]].tok.data , temp , 1);
                        }
                    }
                }

                variable ret = interpret(nodes [ nodes[ndnum].kids.el[nodes[ndnum].kids.size-1] ]  );

                for ( i = stack_size; i > cur_stack_size ; i--){
                    stack_size--;
                    free(hashtable[var_stack[stack_size]]->key);
                    free(hashtable[var_stack[stack_size]]->var.data);
                    free(hashtable[var_stack[stack_size]]);
                    hashtable[var_stack[stack_size]] = NULL;
                }


                ht_offset -= prev_size;
                current_size = prev_size;

                return ret;
            }
        break;
        case 23 :   // uglata zagrada, stvara listu
            ret.type = 23;
            ret.size = Node.kids.size;
            ret.cap = Node.kids.size*2+1;
            ret.data = malloc(sizeof(variable) * ret.cap);
            for (i = 0; i<Node.kids.size; i++){
                variable var1 = interpret( nodes[Node.kids.el[i]] );
                if (interpreter_error) return ret;

                if (nodes[Node.kids.el[i]].tok.type == 36){
                    variable temp;
                    temp.cap = var1.cap;
                    temp.size = var1.size;
                    temp.type = var1.type;
                    temp.fre = true;
                    switch (var1.type){
                        case 17:
                            temp.data = malloc(sizeof(int));
                            memcpy(temp.data, var1.data, sizeof(int) );
                        break;
                        case 23 :
                            temp = copylist(var1);
                        break;
                        case 34 :
                            temp.data = malloc(sizeof(wchar_t) * (var1.size+1) );
                            memcpy(temp.data, var1.data, sizeof(wchar_t) * (var1.size+1) );
                        break;
                        case 35 :
                            temp.data = malloc(sizeof(double));
                            memcpy(temp.data, var1.data, sizeof(double) );
                        break;
                    }
                    memcpy(ret.data+i*sizeof(variable), &temp, sizeof(variable));
                }
                else memcpy(ret.data+i*sizeof(variable), &var1, sizeof(variable));

            }
            return ret;
        break;
        case 28 :       // zbrajanje
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) + *((double*)var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 34){
                    ret.type = 34;
                    ret.size = var1.size + var2.size;
                    ret.cap = ret.size;
                    ret.data = calloc( ret.size+1 , sizeof(wchar_t));
                    wcscat(ret.data, var1.data);
                    wcscat(ret.data, var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    return ret;
                }
                else if (var1.type == 23 && var2.type == 23){
                    ret.type = 23;
                    ret.size = var1.size + var2.size;
                    ret.cap = ret.size * 2;
                    ret.data = malloc( ret.cap * sizeof(variable));
                    if (var1.fre){
                        memcpy(ret.data, var1.data, var1.size*sizeof(variable));
                    }
                    else {
                        variable temp = copylist(var1);
                        memcpy(ret.data, temp.data, var1.size*sizeof(variable));
                    }
                    if (var2.fre){
                        memcpy(ret.data + var1.size * sizeof(variable), var2.data, var2.size*sizeof(variable));
                    }
                    else {
                        variable temp = copylist(var2);
                        memcpy(ret.data + var1.size * sizeof(variable), temp.data, var2.size*sizeof(variable));
                    }
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator + \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 29 :       // negacija ili oduzimanje
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) - *((double*)var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator - \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            if (Node.kids.size == 1){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;

                if (var1.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = -1 * *((double*)var1.data);
                    if (var1.fre)free(var1.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Nevaljani operand %ls za operator - \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 30 :       // djeljenje
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) / *((double*)var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator / \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 31 :       // mnozenje
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) * *((double*)var2.data);
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else if (var1.type == 35 && var2.type == 34){
                    ret.type = 34;
                    int temp = (int)(trunc(*((double*)var1.data)));
                    ret.size = var2.size * temp ;
                    ret.cap = ret.size;
                    ret.data = calloc(ret.size + 1, sizeof(wchar_t) );
                    for (i = 0; i < temp; i++){
                        wcscat(ret.data, var2.data);
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    return ret;
                }
                else if (var1.type == 34 && var2.type == 35){
                    ret.type = 34;
                    int temp = (int)(trunc(*((double*)var2.data)));
                    ret.size = var1.size * temp ;
                    ret.cap = ret.size;
                    ret.data = calloc(ret.size + 1, sizeof(wchar_t) );
                    for (i = 0; i < temp; i++){
                        wcscat(ret.data, var1.data);
                    }
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    return ret;
                }
                else if (var1.type == 23 && var2.type == 35){
                    ret.type = 23;
                    int factor = (int)(trunc(*((double*)var2.data)));
                    ret.size = var1.size * factor;
                    ret.cap = ret.size * 2;
                    ret.data = malloc(sizeof(variable) * ret.cap);
                    int j;
                    for (i = 0; i < factor; i++){
                        variable temp = copylist(var1);
                        for (j = 0; j < var1.size; j++){
                            variable element = ((variable*)temp.data)[j];
                            memcpy(ret.data + (i * var1.size + j) * sizeof(variable), &element, sizeof(variable));
                        }
                        free(temp.data);
                    }
                    if (var1.fre)freelist(&var1, -1);
                    if (var2.fre)free(var2.data);
                    return ret;
                }
                else if (var1.type == 35 && var2.type == 23){
                    ret.type = 23;
                    int factor = (int)(trunc(*((double*)var1.data)));
                    ret.size = var2.size * factor;
                    ret.cap = ret.size * 2;
                    ret.data = malloc(sizeof(variable) * ret.cap);
                    int j;
                    for (i = 0; i < factor; i++){
                        variable temp = copylist(var2);
                        for (j = 0; j < var2.size; j++){
                            variable element = ((variable*)temp.data)[j];
                            memcpy(ret.data + (i * var2.size + j) * sizeof(variable), &element, sizeof(variable));
                        }
                        free(temp.data);
                    }
                    if (var2.fre)freelist(&var2, -1);
                    if (var1.fre)free(var1.data);
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator * \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

            }
        break;
        case 32 :       // mod
            if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));
                    double temp = *((double*)var1.data) - ( trunc ( *((double*)var1.data) / *((double*)var2.data) ) * *((double*)var2.data ) );
                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    return ret;
                }
                else {
                    wchar_t * op1 = type_to_str(var1.type);
                    wchar_t * op2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Nevaljani operandi %ls i %ls za operator % \n", Node.tok.line, op1, op2);
                    free(op1); free(op2);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 34 :       // string tj. niz
            ret.type = 34;
            int len = wcslen(Node.tok.data);
            ret.size = len; ret.cap = len;
            ret.data = malloc(sizeof(wchar_t) * (len + 1));
            memcpy(ret.data, Node.tok.data, (len+1) * sizeof(wchar_t) );
            return ret;
        break;
        case 35 :       // broj
            ret.type = 35;
            ret.size = 1; ret.cap = 1;
            ret.data = malloc(sizeof(double));
            double temp = str_to_num(Node.tok.data);
            memcpy(ret.data, &temp, sizeof(double));
            return ret;
        break;
        case 36 :   // vraca varijablu iz hashtable-a kojoj je kljuc ime tokena cvora
            if (Node.kids.size == 0){

                ret = ht_read(Node.tok.data);
                if (ret.type == 68){
                    wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, Node.tok.data);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                ret.fre = false;
                return ret;

            }
        break;
        case 37 :       // indeksiranje
            if (Node.kids.size == 2){

                if (Node.tok.data[0] == 20){
                    if (nodes[Node.kids.el[0]].tok.type == 37){
                        nodes[Node.kids.el[0]].tok.data[0] = 20;
                        nodes[Node.kids.el[0]].kids.cap = Node.kids.cap + 1;
                        variable indexes = interpret(nodes[Node.kids.el[0]]);
                        if (interpreter_error) return ret;
                        variable var = interpret( nodes[Node.kids.el[1]] );
                        if (interpreter_error) return ret;

                        ret = indexes;
                        ret.cap++;
                        if (var.type != 35){
                            wprintf(L"Linija %d : Indeks može biti samo broj \n", Node.tok.line);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                        int temp = (int)trunc( *((double*)var.data) );
                        memcpy(ret.data + sizeof(int)*(ret.cap-1) , &temp, sizeof(int));
                        if (var.fre)free(var.data);
                        return ret;
                    }
                    else if (nodes[Node.kids.el[0]].tok.type == 36){

                        variable var = interpret( nodes[Node.kids.el[1]] );
                        if (interpreter_error) return ret;
                        if (var.type != 35){
                            wprintf(L"Linija %d : Indeks može biti samo broj \n", Node.tok.line);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                        ret.size = nodes[Node.kids.el[0]].id;
                        ret.cap = 1;
                        ret.data = malloc(sizeof(int) * Node.kids.cap);
                        int temp = (int)trunc( *((double*)var.data) );
                        memcpy(ret.data, &temp, sizeof(int));
                        if (var.fre)free(var.data);
                        return ret;
                    }
                    else{
                        wprintf(L"Linija %d : Vrijednosti mogu biti dodijeljene samo varijablama \n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                }

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 34 && var2.type == 35){
                    ret.type = 34;
                    ret.size = 1; ret.cap = 1;
                    int index = (int)trunc( *((double*)var2.data) );
                    if (index >= var1.size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa niza \n", Node.tok.line);
                        return ret;
                    }
                    ret.data = malloc(sizeof(wchar_t) * 2);
                    wchar_t * temp = malloc(2 * sizeof(wchar_t));
                    temp[0] = ((wchar_t*)var1.data)[ index ] ;
                    temp[1] = L'\0';

                    if (var1.fre)free(var1.data);
                    if (var2.fre)free(var2.data);

                    memcpy(ret.data, temp, sizeof(wchar_t)*2);
                    free(temp);
                    return ret;
                }
                else if (var1.type == 23 && var2.type == 35){

                    int index = (int)trunc( *((double*)var2.data) );
                    if (index >= var1.size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                        return ret;
                    }
                    ret = ((variable*)var1.data)[ index ];
                    if (var1.fre)freelist(&var1, index);
                    else ret.fre = false;
                    if (var2.fre)free(var2.data);
                    return ret;
                }
                else if (var2.type == 35){
                    wchar_t * op1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Nemoguće indeksirati tip podatka %ls \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                else if (var2.type != 35){
                    wprintf(L"Linija %d : Indeks može biti samo broj \n", Node.tok.line);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
        break;
        case 38 :       // break / prekid
            ret.fre = false;
            ret.type = 38; ret.size = 1; ret.cap = 1;
            return ret;
        break;
        case 45 :       // ispis
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -45;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            for (i = 0; i<Node.kids.size; i++){


                if (nodes[Node.kids.el[i]].tok.type == 22) break;
                variable var = interpret( nodes[Node.kids.el[i]] );
                if (interpreter_error) return ret;

                switch (var.type){
                    case 17 :
                        if (*((int*)var.data) > 0) wprintf(L"Funkcija na liniji %d", nodes[ *((int*)var.data) ].tok.line );
                        else if (*((int*)var.data) < 0) wprintf(L"Ugrađena funkcija");
                    break;
                    case 23 :
                        printlist(&var);
                    break;
                    case 34 :
                        wprintf(L"%ls", (wchar_t*)var.data );
                    break;
                    case 35 :

                        if (true){
                            double temp = *((double*)var.data);
                            wchar_t * buffer = calloc(100, sizeof(wchar_t));
                            swprintf(buffer, 70, L"%.15f", temp);
                            int nullpos=49;
                            while (buffer[nullpos] == 0 || buffer[nullpos] == 48) nullpos--;
                            if (buffer[nullpos] == L'.') nullpos--;
                            buffer[nullpos+1] = L'\0';
                            wprintf(L"%ls", buffer );
                            free(buffer);
                        }

                    break;
                    default:
                        wprintf(L"ništa");
                    break;
                }
                if(var.fre){
                    if (var.type == 23)freelist(&var,-1);
                    else if (var.type == 16)ffree(&var);
                    else free(var.data);
                }

            }
            ret.fre = false;
            ret.type = 0; ret.size = 1; ret.cap = 1;
            return ret;
        break;
        case 46 :   // ulaz
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -46;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1 && nodes[Node.kids.el[0]].tok.type == 22 ){
                ret.data = malloc(100 * sizeof(wchar_t));
                ret.size = 0;
                ret.cap = 100;
                wchar_t ch = 32;
                while(WEOF!=(ch=fgetwc(stdin)) && ch != L'\n'){
                    ret.size++;
                    if (ret.size == ret.cap){
                        ret.data = realloc(ret.data, ret.size + 100);
                        ret.cap = ret.cap + 100;
                    }
                    memcpy(ret.data + (ret.size-1) * sizeof(wchar_t), &ch, sizeof(wchar_t));
                }
                ch = fgetwc(stdin);
                ch = L'\0';
                memcpy(ret.data + ret.size * sizeof(wchar_t), &ch, sizeof(wchar_t));
                ret.data = realloc(ret.data, (ret.size+1) * sizeof(wchar_t) );
                ret.cap = ret.size;
                ret.type = 34;
                return ret;

            }
            else{
                wprintf(L"Linija %d : Traženo 0 argumenata, a dano je %d \n", Node.tok.line, Node.kids.size);
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 47 :       // broj()
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -47;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1 && nodes[Node.kids.el[0]].tok.type != 22 ){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (var.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    ret.data = malloc(sizeof(double));

                    double temp = str_to_num((wchar_t*)var.data);
                    memcpy(ret.data, &temp, sizeof(double));
                    if(var.fre)free(var.data);
                    return ret;
                }
                else{
                    wchar_t * arg1 = type_to_str(var.type);
                    wprintf(L"Linija %d : Argument mora biti niz, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if(var.fre)free(var.data);
                ret.type = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 48 :       // niz()
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -48;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1 && nodes[Node.kids.el[0]].tok.type != 22 ){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (var.type == 35){

                    ret.type = 34;
                    double temp = *((double*)var.data);
                    wchar_t * buffer = calloc(50, sizeof(wchar_t));
                    swprintf(buffer, 50 ,L"%.15f", temp);
                    ret.size=49;
                    while (buffer[ret.size] == 0 || buffer[ret.size] == 48) ret.size--;
                    if (buffer[ret.size] == L'.') ret.size--;
                    buffer[ret.size+1] = L'\0';
                    ret.size++;
                    ret.data = malloc( sizeof(wchar_t) * (ret.size+1) );
                    memcpy(ret.data, buffer, sizeof(wchar_t) * (ret.size+1) );
                    ret.cap = ret.size;

                    free(buffer);
                    if(var.fre)free(var.data);
                    return ret;

                }
                else{
                    wchar_t * arg1 = type_to_str(var.type);
                    wprintf(L"Linija %d : Argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if(var.fre)free(var.data);
                ret.type = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 49 :       // abs
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -49;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1  && nodes[Node.kids.el[0]].tok.type != 22 ){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (var.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    double temp = *((double*)var.data);
                    if (temp < 0) temp = -temp;
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));
                    if(var.fre)free(var.data);
                    return ret;
                }
                else{
                    wchar_t * arg1 = type_to_str(var.type);
                    wprintf(L"Linija %d : Argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if(var.fre)free(var.data);
                ret.type = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 50 :       // velicina
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -50;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1  && nodes[Node.kids.el[0]].tok.type != 22 ){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                ret.type = 35;
                ret.size = 1; ret.cap = 1;
                double temp;
                if (var.type == 16) {
                    if ( ((uint8_t*)var.data)[8] == 0 ){
                        temp = -1;
                    }
                    else{
                        int poz = ftell ( ((FILE**)var.data)[0] );
                        fseek( ((FILE**)var.data)[0] , 0, SEEK_END);
                        temp = (double)ftell ( ((FILE**)var.data)[0] );
                        fseek( ((FILE**)var.data)[0] , poz, SEEK_SET);
                    }
                }
                else temp = (double)var.size;
                ret.data = malloc(sizeof(double));
                memcpy(ret.data, &temp, sizeof(double));
                if(var.fre){
                    if (var.type == 23)freelist(&var,-1);
                    if (var.type == 16)ffree(&var);
                    else free(var.data);
                }
                return ret;
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 51 :       // potencija
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -51;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 2){
                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;
                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    double temp = pow( *((double*)var1.data), *((double*)var2.data) );
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));
                    if(var1.fre) free(var1.data);
                    if(var2.fre) free(var2.data);
                    return ret;
                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);

                    if(var1.fre){
                        if (var1.type == 23)freelist(&var1,-1);
                        else free(var1.data);
                    }
                    if(var2.fre){
                        if (var2.type == 23)freelist(&var2,-1);
                        else free(var2.data);
                    }

                    wprintf(L"Linija %d : Argumenti moraju biti brojevi, a dani su %ls i %ls \n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1);free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 52 :       // zaokruzi
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -52;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size >= 2){

                variable var1, var2, var3;
                var3.type = 0;
                var3.fre = false;

                var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (Node.kids.size == 3) { var3 = interpret( nodes[Node.kids.el[2]] ); if (interpreter_error) return ret; }

                if (var1.type == 35 && var2.type == 35){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    double temp1 = *((double*)var1.data);
                    double temp2 = *((double*)var2.data);
                    if (temp2 == trunc(temp2) && temp2 >= 0){
                        double temp = temp1 * pow(10, temp2);
                        if (var3.type != 34){
                            temp = round(temp) / pow(10, temp2);
                        }
                        else {
                            if ( ((wchar_t*)var3.data)[0] == L'v'){
                                temp = ceil(temp) / pow(10, temp2);
                            }
                            else if ( ((wchar_t*)var3.data)[0] == L'm'){
                                temp = floor(temp) / pow(10, temp2);
                            }
                            else {
                                wprintf(L"Linija %d : Nevaljani niz za funkciju zaokruži.\n", Node.tok.line);
                                interpreter_error = true;
                                ret.type = 68; ret.size = 1; ret.cap = 1;
                                ret.fre = false;
                                return ret;
                            }
                        }
                        ret.data = malloc(sizeof(double));
                        memcpy(ret.data, &temp, sizeof(double));
                    }
                    else {
                        wprintf(L"Linija %d : Broj decimala na koliko se zaokružuje mora biti cijel i ne negativan.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }

                }
                else if (var1.type == 35 && var2.type == 34){
                    ret.type = 35;
                    ret.size = 1; ret.cap = 1;
                    double temp = *((double*)var1.data);
                    if ( ((wchar_t*)var2.data)[0] == L'v') temp = ceil(temp);
                    else if ( ((wchar_t*)var2.data)[0] == L'm') temp = floor(temp);
                    else {
                        wprintf(L"Linija %d : Nevaljani niz za funkciju zaokruži.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));

                }
                else if (var1.type != 35){
                    wchar_t * arg1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Prvi argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                else if (var2.type != 35 && var2.type != 34){
                    wchar_t * arg1 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Drugi argument mora biti broj ili niz, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if (var1.fre) free(var1.data);
                if (var2.fre) free(var2.data);
                if (var3.fre) free(var3.data);
                return ret;
            }
            else if (Node.kids.size == 1){
                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (var1.type != 35){
                    wchar_t * arg1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                if (interpreter_error) return ret;
                ret.size = 1; ret.cap = 1;
                ret.type = 35;
                double temp = round(*((double*)var1.data));
                ret.data = malloc(sizeof(double));
                memcpy(ret.data, &temp, sizeof(double));
                if (var1.fre) free(var1.data);
                return ret;
            }
            else{
                wprintf(L"Linija %d : Traženo 1 do 3 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 53 :       //  cijeli dio    matematicke funkcije
        case 54 :       // sin
        case 55 :       // cos
        case 56 :       // tan
        case 57 :       // asin
        case 58 :       // acos
        case 59 :       // atan
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -Node.tok.type;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1  && nodes[Node.kids.el[0]].tok.type != 22 ){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                ret.type = 35;
                ret.size = 1; ret.cap = 1;
                if (var.type == 35){
                    double temp = *((double*)var.data);
                    switch (Node.tok.type){
                        case 53 :
                            temp = trunc(temp);
                        break;
                        case 54 :
                            temp = sin(temp);
                        break;
                        case 55 :
                            temp = cos(temp);
                        break;
                        case 56 :
                            temp = tan(temp);
                        break;
                        case 57 :
                            temp = asin(temp);
                        break;
                        case 58 :
                            temp = acos(temp);
                        break;
                        case 59 :
                            temp = atan(temp);
                        break;
                    }
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));

                    if(var.fre) free(var.data);
                    return ret;
                }
                else {
                    wchar_t * arg = type_to_str(var.type);

                    if(var.fre){
                        if (var.type == 23)freelist(&var,-1);
                        else free(var.data);
                    }

                    wprintf(L"Linija %d : Argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg);
                    interpreter_error = true;
                    free(arg);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 60 :       // log
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -60;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 2){
                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;
                ret.type = 35;
                ret.size = 1; ret.cap = 1;
                if (var1.type == 35 && var2.type == 35){
                    double temp1 = *((double*)var1.data);
                    double temp2 = *((double*)var2.data);
                    double temp;
                    if (temp1 > 0 && temp2 > 0) temp = log(temp2)/log(temp1);
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));

                    if(var1.fre) free(var1.data);
                    if(var2.fre) free(var2.data);
                    return ret;
                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);

                    if(var1.fre){
                        if (var1.type == 23)freelist(&var1,-1);
                        else free(var1.data);
                    }
                    if(var2.fre){
                        if (var2.type == 23)freelist(&var2,-1);
                        else free(var2.data);
                    }

                    wprintf(L"Linija %d : Argumenti moraju biti brojevi, a dani su %ls i %ls \n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1);free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

            }
            else{
                wprintf(L"Linija %d : Tražena 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 61 :       // append / dodaj
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -61;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 2){

                variable * lista;

                int index;
                if (nodes[Node.kids.el[0]].tok.type == 37){
                    nodes[Node.kids.el[0]].tok.data[0] = 20;
                    nodes[Node.kids.el[0]].kids.cap = 1;
                    variable indexes = interpret(nodes[Node.kids.el[0]]);
                    if (interpreter_error) return ret;
                    entry * varptr = ht_read2(nodes[indexes.size].tok.data);
                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[indexes.size].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    if (varptr->var.type != 23){
                        wchar_t * op1 = type_to_str(varptr->var.type);
                        wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                        free(op1);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    variable * temp = ((variable*)varptr->var.data);
                    int list_size;
                    uint8_t temp_type = 23;
                    list_size = varptr->var.size;
                    for (i = 0; i < indexes.cap-1; i++){
                        index = ((int*)indexes.data)[i];
                        if (index >= list_size || index < 0){
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                            return ret;
                        }
                        temp_type = temp[index].type;
                        list_size = temp[index].size;
                        if (temp_type == 23) temp =  (variable *)(temp[index].data);
                        else{
                            wchar_t * op1 = type_to_str(temp_type);
                            wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                            free(op1);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }

                    index = ((int*)indexes.data)[indexes.cap-1];
                    if (index >= list_size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                        return ret;
                    }

                    lista = &(temp[index]);

                }
                else if (nodes[Node.kids.el[0]].tok.type == 36){
                    entry * varptr = ht_read2(nodes[ Node.kids.el[0] ].tok.data);

                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[ Node.kids.el[0] ].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    lista = &(varptr->var);
                }
                if(lista->type != 23){
                    wchar_t * op1 = type_to_str(lista->type);
                    wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

                variable var = interpret( nodes[ Node.kids.el[1] ] );
                if (interpreter_error) return ret;
                if (var.fre){
                    memcpy(lista->data + lista->size*sizeof(variable), &var, sizeof(variable));
                }
                else {
                    variable temp;
                    temp.cap = var.cap; temp.size = var.size;
                    temp.type = var.type;
                    temp.fre = true;
                    switch (var.type){
                        case 17 :
                            temp.data = malloc(sizeof(int));
                            memcpy(temp.data, var.data, sizeof(int) );
                        break;
                        case 23 :
                            temp = copylist(var);
                        break;
                        case 34 :
                            temp.data = malloc(sizeof(wchar_t) * (var.size+1) );
                            memcpy(temp.data, var.data, sizeof(wchar_t) * (var.size+1) );
                        break;
                        case 35 :
                            temp.data = malloc(sizeof(double));
                            memcpy(temp.data, var.data, sizeof(double) );
                        break;
                    }
                    memcpy(lista->data + lista->size*sizeof(variable), &temp, sizeof(variable));
                }
                lista->size++;
                if (lista->size == lista->cap){
                    lista->cap = lista->cap * 2;
                    lista->data = realloc(lista->data, lista->cap*sizeof(variable));
                }
                ret.type = 0;
                ret.size = 1; ret.cap = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 62 :      // pop / ukloni
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -62;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size >= 1){

                variable * lista;

                int index;
                if (nodes[Node.kids.el[0]].tok.type == 37){
                    nodes[Node.kids.el[0]].tok.data[0] = 20;
                    nodes[Node.kids.el[0]].kids.cap = 1;
                    variable indexes = interpret(nodes[Node.kids.el[0]]);
                    if (interpreter_error) return ret;
                    entry * varptr = ht_read2(nodes[indexes.size].tok.data);
                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[indexes.size].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    if (varptr->var.type != 23){
                        wchar_t * op1 = type_to_str(varptr->var.type);
                        wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                        free(op1);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    variable * temp = ((variable*)varptr->var.data);
                    int list_size;
                    uint8_t temp_type = 23;
                    list_size = varptr->var.size;
                    for (i = 0; i < indexes.cap-1; i++){
                        index = ((int*)indexes.data)[i];
                        if (index >= list_size || index < 0){
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                            return ret;
                        }
                        temp_type = temp[index].type;
                        list_size = temp[index].size;
                        if (temp_type == 23) temp =  (variable *)(temp[index].data);
                        else{
                            wchar_t * op1 = type_to_str(temp_type);
                            wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                            free(op1);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }

                    index = ((int*)indexes.data)[indexes.cap-1];
                    if (index >= list_size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                        return ret;
                    }

                    lista = &(temp[index]);

                }
                else if (nodes[Node.kids.el[0]].tok.type == 36){
                    entry * varptr = ht_read2(nodes[ Node.kids.el[0] ].tok.data);

                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[ Node.kids.el[0] ].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    lista = &(varptr->var);
                }
                if(lista->type != 23){
                    wchar_t * op1 = type_to_str(lista->type);
                    wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

                if (lista->size > 0 && Node.kids.size == 1){
                    lista->size--;
                    variable element = ((variable*)lista->data)[lista->size];
                    switch (element.type){
                        case 17 :
                        case 34 :
                        case 35 :
                            free(element.data);
                        break;
                        case 23 :
                            freelist(&element ,-1);
                        break;
                    }
                }
                else if (lista->size == 0){
                    wprintf(L"Linija %d : Nemoguće uklanjati elemente iz prazne liste.\n", Node.tok.line);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                else if (Node.kids.size == 2){

                    variable var = interpret( nodes[ Node.kids.el[1] ] );
                    if (interpreter_error) return ret;

                    if (var.type == 35 && lista->size > 0){
                        int index = (int)(trunc(*((double*)var.data)));
                        lista->size--;

                        if (index <= lista->size && index >= 0){
                            variable element = ((variable*)lista->data)[index];
                            switch (element.type){
                                case 17 :
                                case 34 :
                                case 35 :
                                    free(element.data);
                                break;
                                case 23 :
                                    freelist(&element ,-1);
                                break;
                            }
                            if (index != lista->size){
                                memcpy(lista->data + index*sizeof(variable), lista->data + (index + 1)*sizeof(variable), sizeof(variable) * (lista->size - index) );
                            }
                        }
                        else {
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            wprintf(L"Linija %d : Indeks uklanjanja je izvan dometa liste \n", Node.tok.line);
                            return ret;
                        }
                    }
                    else if(var.type != 35){
                        wchar_t * op1 = type_to_str(var.type);
                        wprintf(L"Linija %d : Drugi argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, op1);
                        free(op1);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    else{
                        wprintf(L"Linija %d : Nemoguće uklanjati elemente iz prazne liste.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                }

                ret.type = 0;
                ret.size = 1; ret.cap = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Traženo 1 ili 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 63 :   // insert / umetni
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -63;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 3){

                variable * lista;

                int index;
                if (nodes[Node.kids.el[0]].tok.type == 37){
                    nodes[Node.kids.el[0]].tok.data[0] = 20;
                    nodes[Node.kids.el[0]].kids.cap = 1;
                    variable indexes = interpret(nodes[Node.kids.el[0]]);
                    if (interpreter_error) return ret;
                    entry * varptr = ht_read2(nodes[indexes.size].tok.data);
                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[indexes.size].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    if (varptr->var.type != 23){
                        wchar_t * op1 = type_to_str(varptr->var.type);
                        wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                        free(op1);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    variable * temp = ((variable*)varptr->var.data);
                    int list_size;
                    uint8_t temp_type = 23;
                    list_size = varptr->var.size;
                    for (i = 0; i < indexes.cap-1; i++){
                        index = ((int*)indexes.data)[i];
                        if (index >= list_size || index < 0){
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                            return ret;
                        }
                        temp_type = temp[index].type;
                        list_size = temp[index].size;
                        if (temp_type == 23) temp =  (variable *)(temp[index].data);
                        else{
                            wchar_t * op1 = type_to_str(temp_type);
                            wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                            free(op1);
                            interpreter_error = true;
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }

                    index = ((int*)indexes.data)[indexes.cap-1];
                    if (index >= list_size || index < 0){
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        wprintf(L"Linija %d : Indeks je izvan dometa liste \n", Node.tok.line);
                        return ret;
                    }

                    lista = &(temp[index]);

                }
                else if (nodes[Node.kids.el[0]].tok.type == 36){
                    entry * varptr = ht_read2(nodes[ Node.kids.el[0] ].tok.data);

                    if (varptr == NULL){
                        wprintf(L"Linija %d : Ime \"%ls\" nije definirano \n", Node.tok.line, nodes[ Node.kids.el[0] ].tok.data);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    lista = &(varptr->var);
                }
                if(lista->type != 23){
                    wchar_t * op1 = type_to_str(lista->type);
                    wprintf(L"Linija %d : Prvi argument mora biti lista, a dan(a) je %ls \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }

                variable var1 = interpret( nodes[ Node.kids.el[1] ] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[ Node.kids.el[2] ] );
                if (interpreter_error) return ret;

                if (var1.type == 35){
                    int index = (int)(trunc(*((double*)var1.data)));
                    if ( index >= lista->size ){

                        if (var2.fre){
                            memcpy(lista->data + lista->size*sizeof(variable), &var2, sizeof(variable));
                        }
                        else {
                            variable temp;
                            temp.cap = var2.cap; temp.size = var2.size;
                            temp.type = var2.type;
                            temp.fre = true;
                            switch (var2.type){
                                case 17 :
                                    temp.data = malloc(sizeof(int));
                                    memcpy(temp.data, var2.data, sizeof(int) );
                                break;
                                case 23 :
                                    temp = copylist(var2);
                                break;
                                case 34 :
                                    temp.data = malloc(sizeof(wchar_t) * (var2.size+1) );
                                    memcpy(temp.data, var2.data, sizeof(wchar_t) * (var2.size+1) );
                                break;
                                case 35 :
                                    temp.data = malloc(sizeof(double));
                                    memcpy(temp.data, var2.data, sizeof(double) );
                                break;
                            }
                            memcpy(lista->data + lista->size*sizeof(variable), &temp, sizeof(variable));
                        }
                        lista->size++;
                        if (lista->size == lista->cap){
                            lista->cap = lista->cap * 2;
                            lista->data = realloc(lista->data, lista->cap*sizeof(variable));
                        }
                    }
                    else if ( index >= 0 ){

                        memcpy(lista->data + (index+1) * sizeof(variable), lista->data + index * sizeof(variable), (lista->size - index) * sizeof(variable) );
                        if (var2.fre){
                            memcpy(lista->data + index * sizeof(variable), &var2, sizeof(variable));
                        }
                        else {
                            variable temp;
                            temp.cap = var2.cap; temp.size = var2.size;
                            temp.type = var2.type;
                            temp.fre = true;
                            switch (var2.type){
                                case 17 :
                                    temp.data = malloc(sizeof(int));
                                    memcpy(temp.data, var2.data, sizeof(int) );
                                break;
                                case 23 :
                                    temp = copylist(var2);
                                break;
                                case 34 :
                                    temp.data = malloc(sizeof(wchar_t) * (var2.size+1) );
                                    memcpy(temp.data, var2.data, sizeof(wchar_t) * (var2.size+1) );
                                break;
                                case 35 :
                                    temp.data = malloc(sizeof(double));
                                    memcpy(temp.data, var2.data, sizeof(double) );
                                break;
                            }
                            memcpy(lista->data + index * sizeof(variable), &var2, sizeof(variable));
                        }

                        lista->size++;
                        if (lista->size == lista->cap){
                            lista->cap = lista->cap * 2;
                            lista->data = realloc(lista->data, lista->cap*sizeof(variable));
                        }

                    }
                    else{
                        wprintf(L"Linija %d : Drugi argument mora biti pozitivan.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                }
                else{
                    wchar_t * op1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Drugi argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, op1);
                    free(op1);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                ret.type = 0;
                ret.size = 1; ret.cap = 0;
                ret.fre = false;
                return ret;
            }
            else{
                wprintf(L"Linija %d : Tražena 3 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 64 :       // red / ord
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -64;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (var.type == 34 && var.size == 1){
                    double temp = ((wchar_t*)var.data)[0] ;
                    ret.type = 35;
                    ret.cap = 1; ret.size = 1;
                    ret.data = malloc(sizeof(double));
                    memcpy(ret.data, &temp, sizeof(double));
                    if (var.fre)  free(var.data);
                    return ret;
                }
                else if (var.type == 34 && var.size != 1){
                    wprintf(L"Linija %d : Dani niz mora biti veličine 1 \n", Node.tok.line);
                    interpreter_error = true;
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
                else {
                    wchar_t * arg1 = type_to_str(var.type);
                    wprintf(L"Linija %d : Argument mora biti niz, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 65 :       // znak / chr
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -65;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 1){
                variable var = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                if (var.type == 35){
                    wchar_t temp = (unsigned int)trunc( *((double*)var.data) );
                    ret.type = 34;
                    ret.cap = 1; ret.size = 1;
                    ret.data = malloc( sizeof(wchar_t) * 2 );
                    memcpy(ret.data, &temp, sizeof(double));
                    wchar_t nullchar = L'\0';
                    memcpy(ret.data + sizeof(wchar_t), &nullchar, sizeof(wchar_t) );
                    if (var.fre)  free(var.data);
                    return ret;
                }
                else {
                    wchar_t * arg1 = type_to_str(var.type);
                    wprintf(L"Linija %d : Argument mora biti broj, a dan(a) je %ls \n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 66 :       // random / nasumicno
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -66;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            if (Node.kids.size == 2){
                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;
                if (var1.type == 35 && var2.type == 35){
                    int temp1 = (int)trunc( *((double*)var1.data) );
                    int temp2 = (int)trunc( *((double*)var2.data) );
                    double temp;
                    int razlika = temp2 - temp1;
                    if (razlika > 0){
                        temp = rand()%(razlika+1) + temp1;
                        ret.type = 35;
                        ret.cap = 1; ret.size = 1;
                        ret.data = malloc( sizeof(double) );
                        memcpy(ret.data, &temp, sizeof(double));
                    }
                    else {
                        wprintf(L"Linija %d : Drugi argument mora biti veći od prvog\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    if (var1.fre)  free(var1.data);
                    if (var2.fre)  free(var2.data);
                    return ret;
                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti brojevi, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 67 :       // otvori / open
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -67;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 34 && var2.type == 34){

                    ret.type = 16;
                    FILE * filep;
                    uint8_t mode = 0;
                    if (!wcscmp((wchar_t*)var2.data, L"č")) { filep = _wfopen((wchar_t*)var1.data, L"r, ccs=UTF-8"); mode = 10; }
                    else if (!wcscmp((wchar_t*)var2.data, L"č+")) { filep = _wfopen((wchar_t*)var1.data, L"r+, ccs=UTF-8"); mode = 11; }
                    else if (!wcscmp((wchar_t*)var2.data, L"čb")) { filep = _wfopen((wchar_t*)var1.data, L"rb"); mode = 12; }
                    else if (!wcscmp((wchar_t*)var2.data, L"č+b")) { filep = _wfopen((wchar_t*)var1.data, L"r+b"); mode = 13; }

                    else if (!wcscmp((wchar_t*)var2.data, L"p")) { filep = _wfopen((wchar_t*)var1.data, L"w, ccs=UTF-8"); mode = 20; }
                    else if (!wcscmp((wchar_t*)var2.data, L"p+")) { filep = _wfopen((wchar_t*)var1.data, L"w+, ccs=UTF-8"); mode = 21; }
                    else if (!wcscmp((wchar_t*)var2.data, L"pb")) { filep = _wfopen((wchar_t*)var1.data, L"wb"); mode = 22; }
                    else if (!wcscmp((wchar_t*)var2.data, L"p+b")) { filep = _wfopen((wchar_t*)var1.data, L"w+b"); mode = 23; }

                    else if (!wcscmp((wchar_t*)var2.data, L"d")) { filep = _wfopen((wchar_t*)var1.data, L"a, ccs=UTF-8"); mode = 30; }
                    else if (!wcscmp((wchar_t*)var2.data, L"d+")) { filep = _wfopen((wchar_t*)var1.data, L"a+, ccs=UTF-8"); mode = 31; }
                    else if (!wcscmp((wchar_t*)var2.data, L"db")) { filep = _wfopen((wchar_t*)var1.data, L"ab"); mode = 32; }
                    else if (!wcscmp((wchar_t*)var2.data, L"d+b")) { filep = _wfopen((wchar_t*)var1.data, L"a+b"); mode = 33; }
                    else {
                        wprintf(L"Linija %d : Nevaljani način otvaranja datoteke\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }

                    if (filep == NULL) mode = 0;

                    ret.data = calloc(9, 1);
                    memcpy(ret.data + 8, &mode, sizeof(uint8_t) );
                    if (mode) memcpy(ret.data, &filep, sizeof(FILE *) );

                    ret.cap = 0;
                    fseek(filep, 0, SEEK_END);
                    ret.size = ftell(filep);
                    rewind(filep);
                    if (var1.fre)  free(var1.data);
                    if (var2.fre)  free(var2.data);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti nizovi, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 70:        // zatvori / close
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -70;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 1){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;

                if (var1.type == 16){

                    ret.type = 0;
                    ret.fre = false;
                    if ( ((uint8_t*)var1.data)[8] != 0 ){
                        fclose( ((FILE**)var1.data)[0] );
                        uint8_t nula = 0;
                        memcpy(var1.data + 8, &nula, 1);
                    }

                    if (var1.fre) free(var1.data);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Argument mora biti datoteka, a dan je %ls.\n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 71:        // poz / ftell
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -71;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 1){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;

                if (var1.type == 16){

                    ret.type = 35;
                    ret.data = malloc(sizeof(double));
                    ret.cap = 1; ret.size = 1;
                    if ( ((uint8_t*)var1.data)[8] != 0 ){

                        double temp = (double)ftell( ((FILE**)var1.data)[0] );
                        memcpy(ret.data, &temp, sizeof(double));

                    }
                    else {
                        double temp = -1;
                        memcpy(ret.data, &temp, sizeof(double));
                    }
                    if (var1.fre) ffree(&var1);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Argument mora biti datoteka, a dan je %ls.\n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argument, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 72 :       // pisi / write
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -72;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 16){

                    if ( ((uint8_t*)var1.data)[8] == 0 ) {
                        wprintf(L"Linija %d : Datoteka nije otvorena.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    else if ( (((uint8_t*)var1.data)[8] % 10 == 2 || ((uint8_t*)var1.data)[8] % 10 == 3) && ((uint8_t*)var1.data)[8] != 12 ){
                        if (var2.type == 23){
                            bool error = false;
                            for (i = 0; i < var2.size; i++){
                                variable temp = ((variable*)var2.data)[i];
                                if (temp.type = 35){
                                    int intbyte = (int)(*((double*)temp.data));
                                    if ( intbyte <= 255 && intbyte >= 0 ){
                                        uint8_t byte = (uint8_t)intbyte;
                                        fwrite( &byte, 1, 1, ((FILE**)var1.data)[0] );
                                    }
                                    else {
                                        error = true;
                                        break;
                                    }
                                }
                                else {
                                    error = true;
                                    break;
                                }
                            }
                            if (error){
                                wprintf(L"Linija %d : Nemoguće zapisati danu listu u datoteku.\n", Node.tok.line);
                                interpreter_error = true;
                                ret.type = 68; ret.size = 1; ret.cap = 1;
                                ret.fre = false;
                                return ret;
                            }
                        }
                        else {
                            wchar_t * arg1 = type_to_str(var1.type);
                            wchar_t * arg2 = type_to_str(var2.type);
                            wprintf(L"Linija %d : Argumenti moraju biti datoteka i lista, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                            interpreter_error = true;
                            free(arg1); free(arg2);
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }
                    else if ( (((uint8_t*)var1.data)[8] % 10 == 0 || ((uint8_t*)var1.data)[8] % 10 == 1) && ((uint8_t*)var1.data)[8] != 10 ){
                        if (var2.type == 34){
                            fwrite( var2.data, sizeof(wchar_t), var2.size, ((FILE**)var1.data)[0] );
                        }
                        else {
                            wchar_t * arg1 = type_to_str(var1.type);
                            wchar_t * arg2 = type_to_str(var2.type);
                            wprintf(L"Linija %d : Argumenti moraju biti datoteka i niz, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                            interpreter_error = true;
                            free(arg1); free(arg2);
                            ret.type = 68; ret.size = 1; ret.cap = 1;
                            ret.fre = false;
                            return ret;
                        }
                    }
                    else {
                        wprintf(L"Linija %d : Datoteka nije otvorena za pisanje.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    ret.type = 0;
                    ret.fre = false;
                    if (var1.fre) ffree(&var1);
                    if (var2.fre){
                        if (var2.type == 23) freelist(&var2, -1);
                        else if (var2.type == 16) ffree(&var2);
                        else free(var2.data);
                    }
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti datoteka i niz ili lista, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 73 :       // citaj / read
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -73;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 16 && var2.type == 35){

                    if ( ((uint8_t*)var1.data)[8] == 0 ) {
                        wprintf(L"Linija %d : Datoteka nije otvorena.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    else if ( (((uint8_t*)var1.data)[8] % 10 == 2 || ((uint8_t*)var1.data)[8] % 10 == 3 ) && ((uint8_t*)var1.data)[8] != 22 && ((uint8_t*)var1.data)[8] != 32 ){
                        uint8_t * bytes = malloc( ((int)(*((double*)var2.data)) + 1) * sizeof(uint8_t) );
                        ret.type = 23;
                        ret.size = fread(bytes, sizeof(uint8_t), (int)(*((double*)var2.data)), ((FILE**)var1.data)[0]);
                        ret.cap = ret.size * 2;
                        ret.data = malloc(ret.cap * sizeof(variable));
                        for (i = 0; i < ret.size; i++){
                            variable temp;
                            temp.type = 35;
                            temp.size = 1; temp.cap = 1;
                            temp.fre = true;
                            temp.data = malloc(sizeof(double));
                            double byte = (double)(bytes[i]);
                            memcpy( temp.data, &byte, sizeof(double) );
                            memcpy( ret.data + sizeof(variable) * i, &temp, sizeof(variable) );
                        }
                        free(bytes);

                    }
                    else if ( (((uint8_t*)var1.data)[8] % 10 == 0 || ((uint8_t*)var1.data)[8] % 10 == 1 ) && ((uint8_t*)var1.data)[8] != 20 && ((uint8_t*)var1.data)[8] != 30 ){
                        wchar_t * buffer = malloc( ((int)(*((double*)var2.data)) + 1) * sizeof(wchar_t) );
                        ret.type = 34;
                        ret.size = fread(buffer, sizeof(wchar_t), (int)(*((double*)var2.data)), ((FILE**)var1.data)[0]);
                        ret.cap = ret.size;
                        buffer[ret.size] = L'\0';
                        ret.data = realloc(buffer, (ret.size + 1) * sizeof(wchar_t));
                    }
                    else {
                        wprintf(L"Linija %d : Datoteka nije otvorena za čitanje.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }

                    if (var1.fre) ffree(&var1);
                    if (var2.fre) free(var2.data);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti datoteka i broj, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 74 :       // citaj liniju / read line
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -74;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 1){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;

                if (var1.type == 16){

                    if ( ((uint8_t*)var1.data)[8] == 0 ) {
                        wprintf(L"Linija %d : Datoteka nije otvorena.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    else if ( (((uint8_t*)var1.data)[8] % 10 == 0 || ((uint8_t*)var1.data)[8] % 10 == 1 ) && ((uint8_t*)var1.data)[8] != 20 && ((uint8_t*)var1.data)[8] != 30 ){
                        ret.data = malloc(100 * sizeof(wchar_t));
                        ret.size = 0;
                        ret.cap = 100;
                        wchar_t ch = 32;
                        while(WEOF!=(ch=fgetwc(((FILE**)var1.data)[0])) && ch != L'\n'){
                            ret.size++;
                            if (ret.size == ret.cap){
                                ret.data = realloc(ret.data, ret.size + 100);
                                ret.cap = ret.cap + 100;
                            }
                            memcpy(ret.data + (ret.size-1) * sizeof(wchar_t), &ch, sizeof(wchar_t));
                        }
                        ch = L'\0';
                        memcpy(ret.data + ret.size * sizeof(wchar_t), &ch, sizeof(wchar_t));
                        ret.data = realloc(ret.data, (ret.size+1) * sizeof(wchar_t) );
                        ret.cap = ret.size;
                        ret.type = 34;
                    }
                    else {
                        wprintf(L"Linija %d : Datoteka nije otvorena za čitanje linija.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }

                    if (var1.fre) ffree(&var1);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wprintf(L"Linija %d : Argument mora biti datoteka, a dan je %ls .\n", Node.tok.line, arg1);
                    interpreter_error = true;
                    free(arg1);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Tražen 1 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 75 :       // trazi / fseek
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -75;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                if (var1.type == 16 && var2.type == 35){

                    if ( ((uint8_t*)var1.data)[8] == 0 ) {
                        wprintf(L"Linija %d : Datoteka nije otvorena.\n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    ret.type = 0;
                    ret.size = 1; ret.cap = 1;
                    ret.fre = false;

                    fseek(((FILE**)var1.data)[0], (int)(*((double*)var2.data)), SEEK_SET );

                    if (var1.fre) ffree(&var1);
                    if (var2.fre) free(var2.data);
                    return ret;

                }
                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti datoteka i broj, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        case 76 :       // razdvoji / split
            if (Node.kids.size == 0){
                ret.type = 17;
                int temp = -76;
                ret.size = 1; ret.cap = 1;
                ret.data = malloc(sizeof(int));
                memcpy(ret.data, &temp, sizeof(int));
                return ret;
            }
            else if (Node.kids.size == 2){

                variable var1 = interpret( nodes[Node.kids.el[0]] );
                if (interpreter_error) return ret;
                variable var2 = interpret( nodes[Node.kids.el[1]] );
                if (interpreter_error) return ret;

                wchar_t strend = L'\0';

                if (var1.type == 34 && var2.type == 35){

                    int index = (int)(*((double*)var2.data));
                    if ( index < 0 || index > var1.size) {
                        wprintf(L"Linija %d : Indeks je izvan dometa niza. \n", Node.tok.line);
                        interpreter_error = true;
                        ret.type = 68; ret.size = 1; ret.cap = 1;
                        ret.fre = false;
                        return ret;
                    }
                    ret.type = 23;
                    ret.size = 2;
                    ret.cap = 4;
                    ret.data = malloc( ret.cap * sizeof(variable) );

                    variable str1, str2;
                    str1.type = 34;
                    str1.size = index; str1.cap = index;
                    str1.fre = true;
                    str1.data = malloc( sizeof(wchar_t) * (str1.size + 1) );
                    memcpy(str1.data, var1.data, str1.size * sizeof(wchar_t) );
                    memcpy(str1.data + str1.size * sizeof(wchar_t) , &strend, sizeof(wchar_t) );

                    str2.type = 34;
                    str2.size = var1.size - index; str2.cap = var1.size - index;
                    str2.fre = true;
                    str2.data = malloc( sizeof(wchar_t) * (str2.size + 1) );
                    memcpy(str2.data, var1.data + index * sizeof(wchar_t), str2.size * sizeof(wchar_t) );
                    memcpy(str2.data + str2.size * sizeof(wchar_t) , &strend, sizeof(wchar_t) );

                    memcpy(ret.data, &str1, sizeof(variable) );
                    memcpy(ret.data + sizeof(variable), &str2, sizeof(variable) );

                    if (var1.fre) free(var1.data);
                    if (var2.fre) free(var2.data);
                    return ret;

                }
                if (var1.type == 34 && var2.type == 34){

                    ret.type = 23;
                    ret.size = 0;
                    ret.cap = var1.size + 10;
                    ret.data = malloc( sizeof(variable) * ret.cap);

                    variable str;
                    str.size = 0;
                    str.cap = 0;
                    str.fre = true;
                    str.type = 34;
                    str.data = malloc( sizeof(wchar_t) * (var1.size + 1) );
                    bool split = false;
                    while ( i < var1.size ){

                        if ( !wcsncmp( var1.data + i * sizeof(wchar_t), var2.data, var2.size ) ){
                            i += var2.size;
                            if (str.size != 0){
                                str.cap = str.size;
                                str.data = realloc(str.data, (str.size + 1) * sizeof(wchar_t) );
                                memcpy(ret.data + ret.size * sizeof(variable), &str, sizeof(variable));
                                ret.size++;
                                str.data = malloc( sizeof(wchar_t) * (var1.size + 1) );
                                str.size = 0;
                            }
                            split = true;
                        }
                        if ( !split || !var2.size ){
                            memcpy( str.data + str.size * sizeof(wchar_t), var1.data + i * sizeof(wchar_t), sizeof(wchar_t));
                            str.size++;
                            memcpy( str.data + str.size * sizeof(wchar_t), &strend, sizeof(wchar_t));
                            i++;
                        }
                        split = false;
                    }
                    if (str.size != 0){
                        str.cap = str.size;
                        str.data = realloc(str.data, (str.size + 1) * sizeof(wchar_t) );
                        memcpy(ret.data + ret.size * sizeof(variable), &str, sizeof(variable));
                        ret.size++;
                        str.data = malloc( sizeof(wchar_t) * (var1.size + 1) );
                        str.size = 0;
                    }


                    if (var1.fre) free(var1.data);
                    if (var2.fre) free(var2.data);
                    return ret;
                }

                else {
                    wchar_t * arg1 = type_to_str(var1.type);
                    wchar_t * arg2 = type_to_str(var2.type);
                    wprintf(L"Linija %d : Argumenti moraju biti nizovi ili niz i broj, a dani su %ls i %ls.\n", Node.tok.line, arg1, arg2);
                    interpreter_error = true;
                    free(arg1); free(arg2);
                    ret.type = 68; ret.size = 1; ret.cap = 1;
                    ret.fre = false;
                    return ret;
                }
            }
            else{
                wprintf(L"Linija %d : Traženo 2 argumenta, a dan(o) je %d \n", Node.tok.line, Node.kids.size - (nodes[Node.kids.el[0]].tok.type == 22) );
                interpreter_error = true;
                ret.type = 68; ret.size = 1; ret.cap = 1;
                ret.fre = false;
                return ret;
            }
        break;
        default :
            ret.fre = false;
            ret.type = 0;
            return ret;
        break;
    }
    ret.fre = false;
    ret.type = 0;
    return ret;

}

int assignment_count = 0;  // brojac dodjela

void parse(){       // zapocinje recursive descent parser koji od array-a tokena stvara stablo od cvorova
    node masternode;                        // prvi cvor
    masternode.tok.type = 25;
    masternode.tok.line = 0;
    masternode.id = 0;
    masternode.parent = 0;
    init(&masternode.kids , 3);
    nodes[0] = masternode; nodesptr++;

    while (tokenptr < toknum){
        node n = parse_0();
        nodes[n.id].parent = masternode.id;
        append(&nodes[masternode.id].kids , n.id);
    }

    current_size = assignment_count;
    global_size = assignment_count;

    hashtable = malloc(ht_size * sizeof(entry*));      // alokacija memorije za hashtable
    var_stack = malloc(ht_size * sizeof(int));           // alokacija memorije za stack entry-ja
    ht_setup();

    variable out;
    if ( !parser_error )out = interpret(nodes[0]);          // ako parser nije javio pogresku zapocinje se interpretat AST

}
node parse_0(){             // druga u nizu od parser funkcija
    node n1 = parse_1();
    node n3;
    init(&n3.kids , 3);
    while(tokens[tokenptr].type==20){
        n3.tok = tokens[tokenptr];
        tokenptr++;

        if (n1.tok.type == 36)assignment_count++;
        if (n1.tok.type != 36 && n1.tok.type != 37 && !parser_error ) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

        node n2 = parse_1();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids,n1.id); append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1] = n3;

        n1 = n3;

    }
    return n1;
}
node parse_1(){                 // treca u nizu od parser funkcija
    node n1 = parse_2();
    node n3;
    while(tokens[tokenptr].type==10){

        n3.tok = tokens[tokenptr];
        init (&n3.kids, 3);
        tokenptr++;

        node n2 = parse_2();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids, n1.id);append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1]=n3;

        n1 = n3;

    }
    return n1;
}
node parse_2(){                 // cetvrta u nizu od parser funkcija
    node n1 = parse_3();
    node n3;
    while(tokens[tokenptr].type==6){

        n3.tok = tokens[tokenptr];
        init (&n3.kids, 3);
        tokenptr++;

        node n2 = parse_3();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids, n1.id);append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1]=n3;

        n1 = n3;

    }
    return n1;
}
node parse_3(){             // peta u nizu od parser funkcija
    bool pass = false;
    node n1;
    while(tokens[tokenptr].type==8){

        pass = true;

        init(&n1.kids , 3);

        n1.tok = tokens[tokenptr];
        tokenptr++;

        node n2 = parse_3();
        nodes[n2.id].parent = nodesptr;
        n1.id = nodesptr; nodesptr++;
        append(&n1.kids, n2.id);
        n1.parent = -1;

        nodes[nodesptr-1]=n1;

    }
    if (!pass) n1 = parse_4();
    return n1;
}
node parse_4(){                 // sesta u nizu od parser funkcija
    node n1 = parse_5();
    node n3;
    while(tokens[tokenptr].type==1 || tokens[tokenptr].type==2 || tokens[tokenptr].type==3 || tokens[tokenptr].type==4 || tokens[tokenptr].type==19 || tokens[tokenptr].type==18){

        n3.tok = tokens[tokenptr];
        init (&n3.kids, 3);
        tokenptr++;

        node n2 = parse_5();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids, n1.id);append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1]=n3;

        n1 = n3;

    }
    return n1;
}
node parse_5(){                 // sedma u nizu od parser funkcija
    node n1 = parse_6();
    node n3;
    while(tokens[tokenptr].type==28 || tokens[tokenptr].type==29){

        n3.tok = tokens[tokenptr];
        init (&n3.kids, 3);
        tokenptr++;

        node n2 = parse_6();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids, n1.id);append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1]=n3;

        n1 = n3;

    }
    return n1;
}
node parse_6(){                 // osma u nizu od parser funkcija
    node n1 = parse_7();
    node n3;
    while(tokens[tokenptr].type==5 || tokens[tokenptr].type==30 || tokens[tokenptr].type==31 || tokens[tokenptr].type==32){

        n3.tok = tokens[tokenptr];
        init (&n3.kids, 3);
        tokenptr++;

        node n2 = parse_7();
        nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
        n3.id = nodesptr; nodesptr++;
        append(&n3.kids, n1.id);append(&n3.kids, n2.id);
        n3.parent = -1;

        nodes[nodesptr-1]=n3;

        n1 = n3;

    }
    return n1;
}
node parse_7(){                 // deveta u nizu od parser funkcija
    bool pass = false;
    node n1;
    if(tokens[tokenptr].type==29){

        pass = true;

        init(&n1.kids , 3);

        n1.tok = tokens[tokenptr];
        tokenptr++;

        node n2 = parse_7();
        nodes[n2.id].parent = nodesptr;
        n1.id = nodesptr; nodesptr++;
        append(&n1.kids, n2.id);
        n1.parent = -1;

        nodes[nodesptr-1]=n1;

    }
    if (!pass) n1 = parse_8();
    return n1;
}
node parse_8(){                // deseta u nizu od parser funkcija
    node n1 = parse_9();
    node n3;
    while(tokens[tokenptr].type==21 || tokens[tokenptr].type==23){

        if (tokens[tokenptr].type==23){
            n3.tok = tokens[tokenptr];
            init (&n3.kids, 3);
            n3.tok.type = 37;
            tokenptr++;
            node n2 = parse_0();

            if (tokens[tokenptr].type == 24) tokenptr++;
            else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

            nodes[n1.id].parent = nodesptr;nodes[n2.id].parent = nodesptr;
            n3.id = nodesptr; nodesptr++;
            append(&n3.kids, n1.id);append(&n3.kids, n2.id);
            n3.parent = -1;

            nodes[nodesptr-1]=n3;
        }
        else if (tokens[tokenptr].type==21){
            n3.tok = tokens[tokenptr];
            init (&n3.kids, 3);
            n3.id = nodesptr; nodesptr++;
            n3.parent = -1;
            nodes[nodesptr-1]=n3;

            do{

                tokenptr++;
                if (tokens[tokenptr].type == 22){
                    if (tokens[tokenptr-1].type == 27 && !parser_error){wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true; break;}
                    node n2;
                    init(&n2.kids , 3);
                    n2.tok = tokens[tokenptr];
                    n2.parent = n3.id;
                    n2.id = nodesptr; nodesptr++;
                    append(&nodes[n3.id].kids, n2.id);
                    nodes[nodesptr-1]=n2;
                    break;
                }
                node n2 = parse_0();

                nodes[n2.id].parent = n3.id;
                append(&nodes[n3.id].kids, n2.id);


            }while(tokens[tokenptr].type==27);

            if (tokens[tokenptr].type == 22) tokenptr++;
            else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}
            append(&nodes[n3.id].kids, n1.id);
            nodes[n1.id].parent = n3.id;
        }
        n1 = n3;
    }
    return n1;
}
node parse_9(){        // zadnja u nizu od parser funkcija

    node n;

    if (tokens[tokenptr].type == 21){

        tokenptr++;
        n = parse_0();
        if (tokens[tokenptr].type == 22)tokenptr++;
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

    }
    else if (tokens[tokenptr].type == 23){

        init(&n.kids , 3);
        n.tok = tokens[tokenptr]; tokenptr++;
        n.id = nodesptr;nodesptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

        if (tokens[tokenptr].type != 24){

            node n1 = parse_0();
            nodes[n1.id].parent = n.id;
            append(&nodes[n.id].kids, n1.id);

            while (tokens[tokenptr].type == 27){

                tokenptr++; n1 = parse_0();
                nodes[n1.id].parent = n.id;
                append(&nodes[n.id].kids, n1.id);

            }
        }
        if (tokens[tokenptr].type == 24)tokenptr++;
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

    }
    else if (tokens[tokenptr].type == 35 || tokens[tokenptr].type == 34 ){

        init(&n.kids , 3);
        n.id = nodesptr;nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

    }
    else if (tokens[tokenptr].type == 36){

        init(&n.kids , 3);
        n.id = nodesptr;nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

    }

    else if (tokens[tokenptr].type == 13 || tokens[tokenptr].type == 11){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

        node n1 = parse_0();
        nodes[n1.id].parent = n.id;
        append(&nodes[n.id].kids, n1.id);

        if (tokens[tokenptr].type != 25){

            n1 = parse_0();
            nodes[n1.id].parent = n.id;
            append(&nodes[n.id].kids, n1.id);

        }
        else {
            tokenptr++;
            do{

                n1 = parse_0();
                nodes[n1.id].parent = n.id;
                append(&nodes[n.id].kids, n1.id);

            }while (tokens[tokenptr].type != 26);

            tokenptr++;

        }

        if (nodes[n.id].tok.type == 11){

            if (tokens[tokenptr].type == 15){

                node n2;
                init(&n2.kids , 3);
                n2.tok = tokens[tokenptr]; tokenptr++;
                n2.id = nodesptr; nodesptr++;
                n2.parent = n.id;
                nodes[nodesptr-1]=n2;
                append(&nodes[n.id].kids, n2.id);

                if (tokens[tokenptr].type == 25) {
                    tokenptr++;
                    do{

                        n1 = parse_0();
                        nodes[n1.id].parent = n2.id;
                        append(&nodes[n2.id].kids, n1.id);

                    }while (tokens[tokenptr].type != 26);

                    tokenptr++;

                }
                else{

                    n1 = parse_0();
                    nodes[n1.id].parent = n2.id;
                    append(&nodes[n2.id].kids, n1.id);
                }
            }
        }
    }

    else if (tokens[tokenptr].type == 9){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;


        node n1 = parse_0();
        nodes[n1.id].parent = n.id;
        append(&nodes[n.id].kids, n1.id);

        if (tokens[tokenptr].type == 27){
                tokenptr++;
                node n1 = parse_0();
                nodes[n1.id].parent = n.id;
                append(&nodes[n.id].kids, n1.id);
        }
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

        if (tokens[tokenptr].type == 27){
                tokenptr++;
                node n1 = parse_0();
                nodes[n1.id].parent = n.id;
                append(&nodes[n.id].kids, n1.id);
        }
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n" , tokens[tokenptr].line);parser_error = true;}

        if (tokens[tokenptr].type != 25){

            n1 = parse_0();
            nodes[n1.id].parent = n.id;
            append(&nodes[n.id].kids, n1.id);

        }
        else {
            tokenptr++;
            do{

                n1 = parse_0();
                nodes[n1.id].parent = n.id;
                append(&nodes[n.id].kids, n1.id);

            }while (tokens[tokenptr].type != 26);

            tokenptr++;
        }
    }
    else if (tokens[tokenptr].type == 17){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

        assignment_count++;

        int temp_assign_count = assignment_count;

        if (tokens[tokenptr].type == 36){
            node n1;
            init(&n1.kids , 3);
            n1.id = nodesptr; nodesptr++;
            n1.tok = tokens[tokenptr]; tokenptr++;
            n1.parent = n.id;
            nodes[nodesptr-1]=n1;
            append(&nodes[n.id].kids, n1.id);

        }
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n", tokens[tokenptr].line);parser_error = true;}

        if (tokens[tokenptr].type == 21){

            do{
                tokenptr++;
                if (tokens[tokenptr].type == 36){
                    node n1;
                    init(&n1.kids , 3);
                    n1.id = nodesptr; nodesptr++;
                    n1.tok = tokens[tokenptr]; tokenptr++;
                    n1.parent = n.id;
                    nodes[nodesptr-1]=n1;
                    append(&nodes[n.id].kids, n1.id);

                    assignment_count++;
                }

            }while (tokens[tokenptr].type == 27);

        }
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n", tokens[tokenptr].line);parser_error = true;}
        if (tokens[tokenptr].type == 22)tokenptr++;
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n", tokens[tokenptr].line);parser_error = true;}

        if (tokens[tokenptr].type == 25) {
            node n1;
            init(&n1.kids , 3);
            n1.id = nodesptr; nodesptr++;
            n1.tok = tokens[tokenptr]; tokenptr++;
            n1.parent = n.id;
            nodes[nodesptr-1]=n1;
            append(&nodes[n.id].kids, n1.id);
            do{

                node n2 = parse_0();
                nodes[n2.id].parent = n1.id;
                append(&nodes[n1.id].kids, n2.id);

            }while (tokens[tokenptr].type != 26);

            tokenptr++;

        }
        else if (!parser_error) {wprintf(L"pravopisna pogreska na liniji %d\n", tokens[tokenptr].line);parser_error = true;}

        int namesinfunc = assignment_count - temp_assign_count + 1;
        assignment_count = temp_assign_count;
        nodes[n.id].tok.data = malloc(sizeof(int));
        memcpy( nodes[n.id].tok.data, &namesinfunc, sizeof(int) );
    }
    else if (tokens[tokenptr].type == 14){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

        node n1 = parse_0();
        nodes[n1.id].parent = n.id;
        append(&nodes[n.id].kids, n1.id);

    }

    else if (tokens[tokenptr].type == 38){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;

    }
    else if (tokens[tokenptr].type == 69){

        init(&n.kids , 3);
        n.id = nodesptr; nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;
        return n;

    }
    else if (tokens[tokenptr].type >= 45 ){
        init(&n.kids , 3);
        n.id = nodesptr;nodesptr++;
        n.tok = tokens[tokenptr]; tokenptr++;
        n.parent = -1;
        nodes[nodesptr-1]=n;
    }
    else{
        if (!parser_error) wprintf(L"pravopisna pogreska na liniji %d\n", tokens[tokenptr].line);
        parser_error = true;
        init(&n.kids, 3);
        n.tok = tokens[tokenptr];
        n.tok.type = 0;
        n.id = 0; n.parent = 0;
        tokenptr++;
    }
    return n;
}


int main(int argc, char * argv[]){          // main funkcija

    _setmode(_fileno(stdout), _O_U16TEXT);      //  ulaz i izlaz je sada u unicode-u
    _setmode(_fileno(stdin), _O_U16TEXT);
    FILE * file;                      // pointer na datoteku koju otvaramo
    int fsize;
    wchar_t * buffer;
    if (argc == 2) file = fopen(argv[1], "r, ccs=UTF-8");   // ako je program pokrenut s nekom datotekom ona se otvara
    else return 0;                                       // inace program odmah zavrsi s radom

    srand(time(0));     // random seed za funkciju nasumicno

    fseek(file, 0, SEEK_END);       // odredivanje velicine procitane datoteke
    fsize = ftell(file);
    rewind(file);
    buffer = calloc(fsize+1, sizeof(wchar_t));    // alokacija momorije za buffer

    fread(buffer + 1, sizeof(wchar_t), fsize, file);    // prepisivanje datoteke u buffer

    buffer[0] = L'\n';      // prvi znak buffera mora biti new line

    fclose(file);   // datoteka nam vise ne treba pa je zatvaramo

    tokens = calloc(fsize + 1, sizeof(token));  // alociramo memoriju za tokene

    int line_counter = 1;   // brj na kojoj linij se nalayimo dok citamo tekst iz datoteke
    int i = 1;

    bool token_add_prev = true;     // je li token dodan u proslij iteraciji
    while (i<fsize+2){      // ovo je lekser koji tekst iz buffera pretvara u array tokena

        if (buffer[i] == 10) line_counter++;    // ako je znak \n tekst se prebacije na slijedecu liniju

        if (token_add_prev){
            tokens[toknum].data = calloc(1, sizeof(wchar_t));
            token_add_prev = false;
        }
        tokens[toknum].line = line_counter;

        bool token_added = false;

        if (i<fsize-1){

            if (buffer[i] == L'=' && buffer[i+1] == L'=' && !token_added) {tokens[toknum].type=1; token_added = true; i=i+1;}
            else if (buffer[i] == L'!' && buffer[i+1] == L'=' && !token_added) {tokens[toknum].type=2; token_added = true; i=i+1;}
            else if (buffer[i] == L'>' && buffer[i+1] == L'=' && !token_added) {tokens[toknum].type=3; token_added = true; i=i+1;}
            else if (buffer[i] == L'<' && buffer[i+1] == L'=' && !token_added) {tokens[toknum].type=4; token_added = true; i=i+1;}
            else if (buffer[i] == L'/' && buffer[i+1] == L'/' && !token_added) {tokens[toknum].type=5; token_added = true; i=i+1;}
            else if ((buffer[i-1]<=32 || buffer[i-1]==L')' || buffer[i-1]==L']') && buffer[i] == L'I' && (buffer[i+1]<=32 || buffer[i+1]==L'(' || buffer[i+1]==L'[') && !token_added){
                tokens[toknum].type=6; token_added = true;
            }

        }
        if (i<fsize-2){

            if ((buffer[i-1]<=32 || buffer[i-1]==L'(') && buffer[i] == L'N' && buffer[i+1] == L'E' && (buffer[i+2]<=32 || buffer[i+2]==L'(') && !token_added) {
                tokens[toknum].type=8; token_added = true; i=i+1;
            }
            else if (buffer[i-1]<=32 && buffer[i] == L'z' && buffer[i+1] == L'a' && buffer[i+2]<=32 && !token_added) {
                tokens[toknum].type=9; token_added = true; i=i+1;
            }

        }
        if (i<fsize-3){

            if ((buffer[i-1]<=32 || buffer[i-1]==L')' || buffer[i-1]==L']') && buffer[i] == L'I' && buffer[i+1] == L'L' && buffer[i+2] == L'I' && (buffer[i+3]<=32 || buffer[i+3]==L'(') && !token_added) {
                tokens[toknum].type=10; token_added = true; i=i+2;
            }
            else if (buffer[i-1]<=32 && buffer[i] == L'a' && buffer[i+1] == L'k' && buffer[i+2] == L'o' && (buffer[i+3]<=32 || buffer[i+3]==L'(') && !token_added) {
                tokens[toknum].type=11; token_added = true; i=i+2;
            }

        }
        if (i<fsize-5){

            if (buffer[i] == L'd' && buffer[i+1] == L'o' && buffer[i+2] == L'k' && buffer[i+3] == L' ' && buffer[i+4] == L'j' && buffer[i+5] == L'e' && !token_added) {
                    tokens[toknum].type=13; token_added = true; i=i+5;
            }
            else if (buffer[i-1]<=32 && buffer[i] == L'v' && buffer[i+1] == L'r' && buffer[i+2] == L'a' && buffer[i+3] == L't' && buffer[i+4] == L'i' && (buffer[i+5]<=32 || buffer[i+5]==L'(') && !token_added) {
                        tokens[toknum].type=14; token_added = true; i=i+4;
            }
            else if ((buffer[i-1]<=32 || buffer[i-1]==L'}')&& buffer[i] == L'i' && buffer[i+1] == L'n' && buffer[i+2] == L'a' && buffer[i+3] == L'č' && buffer[i+4] == L'e' && (buffer[i+5]<=32 || buffer[i+5]==L'{') && !token_added) {
                    tokens[toknum].type=15; token_added = true; i=i+4;
            }

        }

        if (i<fsize-6){

            if (buffer[i-1]<=32 && buffer[i] == L'p' && buffer[i+1] == L'r' && buffer[i+2] == L'e' && buffer[i+3] == L'k' && buffer[i+4] == L'i' && buffer[i+5] == L'd' && buffer[i+6]<=32 && !token_added) {
                tokens[toknum].type=38; token_added = true; i=i+5;
            }

        }
        if (i<fsize-8){

            if (buffer[i-1]<=32 && buffer[i] == L'f' && buffer[i+1] == L'u' && buffer[i+2] == L'n' && buffer[i+3] == L'k' && buffer[i+4] == L'c' && buffer[i+5] == L'i' && buffer[i+6] == L'j' && buffer[i+7] == L'a' && buffer[i+8]<=32 && !token_added) {
                    tokens[toknum].type=17; token_added = true; i=i+7;
            }

        }

        if (buffer[i] == L'>'  && !token_added) {tokens[toknum].type=18; token_added = true;}
        else if (buffer[i] == L'<'  && !token_added) {tokens[toknum].type=19; token_added = true;}
        else if (buffer[i] == L'='  && !token_added) {tokens[toknum].type=20; token_added = true;}
        else if (buffer[i] == L'('  && !token_added) {tokens[toknum].type=21; token_added = true;}
        else if (buffer[i] == L')'  && !token_added) {tokens[toknum].type=22; token_added = true;}
        else if (buffer[i] == L'['  && !token_added) {tokens[toknum].type=23; token_added = true;}
        else if (buffer[i] == L']'  && !token_added) {tokens[toknum].type=24; token_added = true;}
        else if (buffer[i] == L'{'  && !token_added) {tokens[toknum].type=25; token_added = true;}
        else if (buffer[i] == L'}'  && !token_added) {tokens[toknum].type=26; token_added = true;}
        else if (buffer[i] == L','  && !token_added) {tokens[toknum].type=27; token_added = true;}
        else if (buffer[i] == L'+'  && !token_added) {tokens[toknum].type=28; token_added = true;}
        else if (buffer[i] == L'-'  && !token_added) {tokens[toknum].type=29; token_added = true;}
        else if (buffer[i] == L'/'  && !token_added) {tokens[toknum].type=30; token_added = true;}
        else if (buffer[i] == L'*'  && !token_added) {tokens[toknum].type=31; token_added = true;}
        else if (buffer[i] == L'%'  && !token_added) {tokens[toknum].type=32; token_added = true;}

        if (buffer[i]==39 && !token_added){

            tokens[toknum].type = 34;
            i++;
            int counter = 0;
            while (buffer[i]!=39){
                if (buffer[i] == L'\\'){
                    switch (buffer[i+1]){
                        case L'n':
                            tokens[toknum].data[counter] = L'\n';i++;
                        break;
                        case L't':
                            tokens[toknum].data[counter] = L'\t';i++;
                        break;
                        case L'\\':
                            tokens[toknum].data[counter] = L'\\';i++;
                        break;
                        case L'\'':
                            tokens[toknum].data[counter] = L'\'';i++;
                        break;
                        case L'\"':
                            tokens[toknum].data[counter] = L'\"';i++;
                        break;
                        default:
                            tokens[toknum].data[counter] = L'\\';
                        break;
                    }
                }
                else tokens[toknum].data[counter] = buffer[i];
                if (buffer[i] == L'\n') line_counter++;
                tokens[toknum].data = realloc(tokens[toknum].data, (counter + 2) * sizeof(wchar_t) );
                i++;
                counter++;
            }
            tokens[toknum].data[counter] = L'\0';
            token_added = true;
        }
        if (buffer[i]==L'"' && !token_added){

            tokens[toknum].type = 34;
            i++;
            int counter = 0;
            while (buffer[i]!=L'"'){
                if (buffer[i] == L'\\'){
                    switch (buffer[i+1]){
                        case L'n':
                            tokens[toknum].data[counter] = L'\n';i++;
                        break;
                        case L't':
                            tokens[toknum].data[counter] = L'\t';i++;
                        break;
                        case L'\\':
                            tokens[toknum].data[counter] = L'\\';i++;
                        break;
                        case L'\'':
                            tokens[toknum].data[counter] = L'\'';i++;
                        break;
                        case L'\"':
                            tokens[toknum].data[counter] = L'\"';i++;
                        break;
                        default:
                            tokens[toknum].data[counter] = L'\\';
                        break;
                    }
                }
                else tokens[toknum].data[counter] = buffer[i];
                if (buffer[i] == L'\n') line_counter++;
                tokens[toknum].data = realloc(tokens[toknum].data, (counter + 2) * sizeof(wchar_t) );
                i++;
                counter++;
            }
            tokens[toknum].data[counter] = L'\0';

            token_added = true;
        }
        if (buffer[i]>=48 && buffer[i]<=57 && !token_added){

            tokens[toknum].type = 35;
            int counter = 0;
            while ((buffer[i]>47 && buffer[i]<58) || buffer[i]==L'.'){
                tokens[toknum].data[counter] = buffer[i];
                tokens[toknum].data = realloc(tokens[toknum].data, (counter + 2) * sizeof(wchar_t) );
                i++;
                counter++;
            }
            tokens[toknum].data[counter] = L'\0';

            i--;
            token_added = true;
        }
        if (((buffer[i]>=65 && buffer[i]<=90) || (buffer[i]>=95 && buffer[i]<=122) || buffer[i]==262 || buffer[i]==263 || buffer[i]==268 || buffer[i]==269 || buffer[i]==272 || buffer[i]==273 || buffer[i]==352 || buffer[i]==353 || buffer[i]==381 || buffer[i]==382) && !token_added){

            tokens[toknum].type = 36;
            int counter = 0;
            while ( (buffer[i]>47 && buffer[i]<58) || (buffer[i]>=65 && buffer[i]<=90) || (buffer[i]>=95 && buffer[i]<=122) || buffer[i]==262 || buffer[i]==263 || buffer[i]==268 || buffer[i]==269 || buffer[i]==272 || buffer[i]==273 || buffer[i]==352 || buffer[i]==353 || buffer[i]==381 || buffer[i]==382){
                tokens[toknum].data[counter] = buffer[i];
                tokens[toknum].data = realloc(tokens[toknum].data, (counter + 3) * sizeof(wchar_t) );
                i++;
                counter++;
            }
            tokens[toknum].data[counter] = L'\0';

            if (!wcscmp(tokens[toknum].data,L"ispis")){
                tokens[toknum].type = 45;
            }
            else if (!wcscmp(tokens[toknum].data,L"ulaz")){
                tokens[toknum].type = 46;
            }
            else if (!wcscmp(tokens[toknum].data,L"broj")){
                tokens[toknum].type = 47;
            }
            else if (!wcscmp(tokens[toknum].data,L"niz")){
                tokens[toknum].type = 48;
            }
            else if (!wcscmp(tokens[toknum].data,L"abs")){
                tokens[toknum].type = 49;
            }
            else if (!wcscmp(tokens[toknum].data,L"vel")){
                tokens[toknum].type = 50;
            }
            else if (!wcscmp(tokens[toknum].data,L"pot")){
                tokens[toknum].type = 51;
            }
            else if (!wcscmp(tokens[toknum].data,L"zaokruži")){
                tokens[toknum].type = 52;
            }
            else if (!wcscmp(tokens[toknum].data,L"cijeli_dio")){
                tokens[toknum].type = 53;
            }
            else if (!wcscmp(tokens[toknum].data,L"sin")){
                tokens[toknum].type = 54;
            }
            else if (!wcscmp(tokens[toknum].data,L"cos")){
                tokens[toknum].type = 55;
            }
            else if (!wcscmp(tokens[toknum].data,L"tan")){
                tokens[toknum].type = 56;
            }
            else if (!wcscmp(tokens[toknum].data,L"asin")){
                tokens[toknum].type = 57;
            }
            else if (!wcscmp(tokens[toknum].data,L"acos")){
                tokens[toknum].type = 58;
            }
            else if (!wcscmp(tokens[toknum].data,L"atan")){
                tokens[toknum].type = 59;
            }
            else if (!wcscmp(tokens[toknum].data,L"log")){
                tokens[toknum].type = 60;
            }
            else if (!wcscmp(tokens[toknum].data,L"dodaj")){
                tokens[toknum].type = 61;
            }
            else if (!wcscmp(tokens[toknum].data,L"ukloni")){
                tokens[toknum].type = 62;
            }
            else if (!wcscmp(tokens[toknum].data,L"umetni")){
                tokens[toknum].type = 63;
            }
            else if (!wcscmp(tokens[toknum].data,L"red")){
                tokens[toknum].type = 64;
            }
            else if (!wcscmp(tokens[toknum].data,L"znak")){
                tokens[toknum].type = 65;
            }
            else if (!wcscmp(tokens[toknum].data,L"nasumično")){
                tokens[toknum].type = 66;
            }
            else if (!wcscmp(tokens[toknum].data,L"otvori")){
                tokens[toknum].type = 67;
            }
            else if (!wcscmp(tokens[toknum].data,L"zatvori")){
                tokens[toknum].type = 70;
            }
            else if (!wcscmp(tokens[toknum].data,L"poz")){
                tokens[toknum].type = 71;
            }
            else if (!wcscmp(tokens[toknum].data,L"piši")){
                tokens[toknum].type = 72;
            }
            else if (!wcscmp(tokens[toknum].data,L"čitaj")){
                tokens[toknum].type = 73;
            }
            else if (!wcscmp(tokens[toknum].data,L"čitaj_liniju")){
                tokens[toknum].type = 74;
            }
            else if (!wcscmp(tokens[toknum].data,L"traži")){
                tokens[toknum].type = 75;
            }
            else if (!wcscmp(tokens[toknum].data,L"razdvoji")){
                tokens[toknum].type = 76;
            }
            i--;
            token_added = true;
        }

        if (token_added){toknum++;token_add_prev = true;}
        else token_add_prev = false;


        i++;
    }
    free(buffer);       // buffer na vise ne treba pa oslobadamo memoriju

    tokens = realloc(tokens, sizeof(token) * (toknum + 1) );       // smanjujemo velicinu token pointera jer je zauzimala visak memorije

    nodes = calloc(toknum+2 , sizeof(node) * (toknum + 1) );     // alociranje memorije za array cvorova
    // dodavanje tokena koji oznacava kraj token array-a
    tokens[toknum].line = line_counter;
    tokens[toknum].type = 69;

    parse();    // poyivanje parsera

    wchar_t end_prevent = fgetwc(stdin);       // zaustavlja gasenje programa jer zahtjeva input

    return 0;
}
