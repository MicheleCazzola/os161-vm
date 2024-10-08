# Progetto OS161: C1 - paging
## Introduzione
Il progetto svolto riguarda la realizzazione di un gestore della memoria virtuale, che sfrutti il demand paging e il loading dinamico; il suo nome è _pagevm_, come si nota da uno dei file presenti nel progetto.

I file aggiunti alla configurazione DUMBVM seguono le indicazioni fornite nella traccia del progetto, a cui si aggiunge il file pagevm.c (e relativo header file), contenente le funzioni di base per la gestione della memoria (dettagliate in seguito).

Il progetto è stato svolto nella variante denominata *C1.1*, con page table indipendenti per ogni processo (ulteriori dettagli sono forniti nelle sezioni successive).

## Composizione e suddivisione carichi di lavoro
Il carico di lavoro è stato suddiviso in maniera abbastanza netta tra i componenti del gruppo, utilizzando una repository condivisa su Github:
- Filippo Forte (s322788): address space e gestore della memoria, modifica a file già esistenti in *DUMBVM*;
- Michele Cazzola (s323270): segmenti, page table, statistiche e astrazione per il TLB, con wrapper per funzioni già esistenti;
- Leone Fabio (s330500): coremap e gestione dello swapfile.

Ogni componente del gruppo ha gestito in modo autonomo l'implementazione dei moduli, compresa la cura della documentazione e della propria parte di report, dopo aver concordato inizialmente l'interfaccia definita negli header file e le dipendenze tra essi. 

La comunicazione è avvenuta principalmente con videochiamate a cadenza settimanale, di durata pari a 2-3 ore ciascuna, durante le quali si è discusso il lavoro effettuato ed è stato pianificato quello da svolgere.

## Architettura del progetto
Il progetto è stato realizzato utilizzando una sorta di _layer architecture_, con rare eccezioni, per cui ogni modulo fornisce astrazioni di un livello specifico, con dipendenze (quasi) unidirezionali. La descrizione dei moduli segue il loro livello di astrazione:
- ad alto livello, si interfacciano con moduli già esistenti, tra cui _runprogram.c_, _loadelf.c_, _main.c_, _trap.c_;
- ai livelli inferiori, forniscono servizi intermedi: segmento e page table sono dipendenze dell'address space (uno direttamente, l'altro indirettamente), coremap è dipendenza di diversi moduli (tra cui page table e quelli di livello superiore), analogamente a swapfile;
- altri sono moduli ausiliari: l'astrazione per il TLB è utilizzata in diversi moduli per accedere alle sue funzionalità, le statistiche sono calcolate solo invocando funzioni di interfaccia. 

## Codice sorgente

### Address space

Ogni processo possiede un address space che nel nostro caso viene rappresentato da 3 diversi segmenti: code, data e stack.

#### Struttura Dati

```C
typedef struct {
    ps_t *seg_code;
    ps_t *seg_data;
    ps_t *seg_stack;
} addrspace_t ;
```

#### Implementazione

Le funzioni di addrspace.c sono principalmente funzioni di alto livello che creano, gestiscono e distruggono l'address space appoggiandosi su funzioni di più basso livello implementate in segment.c. I prototipi corrispondenti sono:

```C
addrspace_t *as_create(void);
void as_destroy(addrspace_t *);
int as_copy(addrspace_t *src, addrspace_t **ret);
void as_activate(void);
void as_deactivate(void);
int as_prepare_load(addrspace_t *as);
int as_complete_load(addrspace_t *as);
int as_define_stack(addrspace_t *as, vaddr_t *initstackptr);
int as_define_region(addrspace_t *as,
                                   vaddr_t vaddr, size_t memsize,
                                   size_t file_size,
                                   off_t offset,
                                   struct vnode *v,
                                   int readable,
                                   int writeable,
                                   int executable);
ps_t *as_find_segment(addrspace_t *as, vaddr_t vaddr);
ps_t *as_find_segment_coarse(addrspace_t *as, vaddr_t vaddr);
```

##### Creazione e distruzione

Le funzioni _as_create_ e _as_destroy_ hanno il compito di allocare e liberare lo spazio di memoria necessario per ospitare la struttura dati; _as_destroy_ oltre a distruggere i 3 segmenti corrispondenti tramite la _seg_destroy_, ha anche il compito di chiudere il program file che viene lasciato aperto per permettere il loading dinamico. 

##### Copia e attivazione

Si utilizzano le funzioni:
- _as_copy_: si occupa di creare un nuovo address space e copiarci quello ricevuto come parametro. Si basa sulla _seg_copy_
- _as_activate_: questa funzione viene chiamata in _runprogram_ subito dopo aver creato e settato l'address space del processo. In particolare ha il compito di invalidare le entry del TLB.

##### Define

Le funzioni _as_define_region_ e _as_define_stack_ vengono utilizzate per definire segmento codice e segmento dati (per la _as_define_region_) e segmento stack (per la _as_define_stack_). Esse fungono essenzialmente da wrapper per le relative funzioni di più basso livello. Ciò che aggiungono è il calcolo della dimensione dei relativi segmenti, dato necessario per le funzioni definite in segment.c.

##### Find

Sono presenti 2 funzioni che dato un address space e un indirizzo virtuale permettono di risalire al relativo segmento. Le due funzioni si differenziano nella granularità della ricerca, esse sono _as_find_segment_ e _as_find_segment_coarse_. Entrambe le funzioni hanno il compito di calcolare inizio e fine dei 3 segmenti (code, data e stack) e verificare a quale di questi l'indirizzo passato appartiene.

Rispetto alla funzione as_find_segment, viene utilizzata la versione "coarse" (a granularità più grossolana), che opera a livello di pagina, per gestire il problema dell'indirizzo base virtuale di un segmento non allineato con le pagine. Tuttavia, questa soluzione presenta dei rischi in termini di sicurezza: l'operazione di allineamento con le pagine potrebbe erroneamente considerare alcuni indirizzi virtuali, che in realtà non appartengono a nessun segmento specifico, come appartenenti ad un segmento.

### Gestore della memoria (pagevm)

In questo modulo vengono raccolte e utilizzate molte delle funzioni chiave implementate per questo progetto. Il modulo ha il compito di inizializzare e terminare l'intera gestione della memoria virtuale, oltre a gestire eventuali fault della memoria.

#### Implementazione

Le funzioni implementate in questo modulo sono:

```C
void vm_bootstrap(void);
void vm_shutdown(void);
int vm_fault(int faulttype, vaddr_t faultaddress);
void pagevm_can_sleep(void);
```

#### Inizializzazione e Terminazione

Le funzioni _vm_bootstrap_ e _vm_shutdown_ hanno il compito, rispettivamente, di inizializzare e distruggere tutte le strutture accessorie necessarie per la gestione della memoria virtuale. Tra queste strutture figurano la coremap, il sistema di swap, il TLB e il sistema di raccolta delle statistiche. Queste funzioni fungono essenzialmente da contenitori che richiamano le routine di inizializzazione e terminazione di altri moduli.

- **vm_bootstrap**: Questa funzione viene chiamata durante l'avvio del sistema per configurare l'intero sistema di memoria virtuale. Tra le operazioni principali, resetta il puntatore della vittima nel TLB, inizializza la coremap, imposta il sistema di swap e prepara il modulo di gestione delle statistiche.
  
- **vm_shutdown**: Questa funzione viene invocata durante lo spegnimento del sistema per rilasciare in modo sicuro le risorse utilizzate e stampare le statistiche sull'uso della memoria. Gestisce la chiusura del sistema di swap, della coremap e produce l'output delle statistiche raccolte.

#### Gestione dei Fault

La funzione centrale di questo modulo è _vm_fault_, che si occupa della gestione dei TLB miss.

- **vm_fault**: Questa funzione ha il compito di gestire la creazione di una nuova corrispondenza tra indirizzo fisico e indirizzo virtuale nel TLB ogni volta che si verifica un TLB miss. Il funzionamento della funzione si articola nei seguenti passaggi:

  1. **Verifica del Fault**: La funzione inizia verificando il tipo di fault (ad esempio, read-only, read, write) e assicurandosi che il processo corrente e il suo spazio di indirizzamento siano validi.
  
  2. **Recupero dell'Indirizzo Fisico**: Successivamente, recupera l'indirizzo fisico corrispondente all'indirizzo virtuale in cui si è verificato il fault utilizzando la funzione _seg_get_paddr_.
  
  3. **Gestione della Pagina**: Se il fault è dovuto a una pagina non ancora assegnata o a una pagina precedentemente _swapped out_, viene allocata una nuova pagina. In base al tipo di fault, vengono quindi chiamate le funzioni di basso livello _seg_add_pt_entry_ (per aggiungere la nuova pagina alla tabella delle pagine) o _seg_swap_in_ (per caricare la pagina dallo swapfile).
  
  4. **Aggiornamento del TLB**: Infine, utilizzando un algoritmo round-robin, viene scelta la vittima da sostituire nel TLB e il TLB viene aggiornato con la nuova corrispondenza indirizzo fisico-virtuale.

### Segmento
Un processo ha uno spazio di indirizzamento costituito da diversi segmenti, che sono aree di memoria aventi una semantica comune; nel nostro caso, essi sono tre:
- codice: contiene il codice dell'eseguibile, è read-only; 
- dati: contiene le variabili globali, è read-write;
- stack: contiene gli stack frame delle funzioni chiamate durante l'esecuzione del processo, è read-write.

Essi non sono necessariamente contigui in memoria fisica, pertanto la soluzione adottata è la realizzazione di una page table per ognuno di essi; il numero di pagine necessarie è calcolato a valle della lettura dell'eseguibile, tranne che nel caso dello stack, in cui è costante.

#### Strutture dati
La struttura dati che rappresenta il singolo segmento è definita in _segment.h_ ed è la seguente:
```C
typedef struct  {
    seg_permissions_t permissions;
    size_t seg_size_bytes;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t num_pages;
    size_t seg_size_words;
    struct vnode *elf_vnode;
    pt_t *page_table;
} ps_t;
```
I campi hanno il significato seguente:
- _permissions_: permessi associati al segmento, definiti dal tipo enumerativo:
```C
typedef enum {
    PAGE_RONLY,     /* 0: read-only */
    PAGE_RW,        /* 1: read-write */
    PAGE_EXE,       /* 2: executable */
    PAGE_STACK      /* 3: stack */
} seg_permissions_t;
```
- _seg_size_bytes_: dimensione del segmento nell'eseguibile, in bytes;
- _file_offset_: offset del segmento all'interno dell'eseguibile;
- _base_vaddr_: indirizzo virtuale iniziale del segmento;
- _num_pages_: numero di pagine occupate dal segmento, in memoria;
- _seg_size_words_: memoria occupata dal segmento, considerando le parole intere;
- _elf_vnode_: puntatore al vnode del file eseguibile a cui il segmento appartiene;
- _page_table_: puntatore alla page table associata al segmento

con il vincolo che _seg_size_bytes_ ≤ _seg_size_words_; inoltre, nel caso in cui la dimensione effettiva del segmento sia inferiore alla memoria che esso occupa, il residuo viene completamente azzerato.

#### Implementazione
Le funzioni di questo modulo si occupano di svolgere operazioni a livello segmento, eventualmente agendo da semplici wrapper per funzioni di livello inferiore. Esse sono utilizzate all'interno dei moduli _addrspace_ o _pagevm_; i prototipi sono i seguenti:
```C
ps_t *seg_create(void);
int seg_define(
    ps_t *proc_seg, size_t seg_size_bytes, off_t file_offset,
    vaddr_t base_vaddr, size_t num_pages, size_t seg_size_words,
    struct vnode *elf_vnode, char read, char write, char execute
);
int seg_define_stack(ps_t *proc_seg, vaddr_t base_vaddr, size_t num_pages);
int seg_prepare(ps_t *proc_seg);
int seg_copy(ps_t *src, ps_t **dest);
paddr_t seg_get_paddr(ps_t *proc_seg, vaddr_t vaddr);
void seg_add_pt_entry(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_swap_out(ps_t *proc_seg, off_t swapfile_offset, vaddr_t vaddr);
void seg_swap_in(ps_t *proc_seg, vaddr_t vaddr, paddr_t paddr);
void seg_destroy(ps_t *proc_seg);
```

##### Creazione e distruzione
Si utilizzano le funzioni:
- _seg_create_: crea un nuovo segmento, allocando la memoria necessaria mediante _kmalloc_ e azzerandone tutti i campi, viene invocata alla creazione di un address space;
- _seg_destroy_: distrugge il segmento dato, liberando anche la memoria detenuta dalla propria page table, viene invocata alla distruzione dell'address space a cui il segmento appartiene.

##### Definizione, preparazione e copia
Si utilizzano le funzioni:
- _seg_define_: definisce il valore dei campi del segmento dato, utilizzando i parametri passati dal chiamante (ovvero _as_define_region_); essi non includono informazioni riguardanti la page table, che viene definita in seguito; tale funzione è utilizzata solo per le regioni di codice e dati, non per lo stack;
- _seg_define_stack_: come la precedente, ma utilizzata solo per lo stack e invocata da _as_define_stack_; per la natura dello stack:
    * esso non esiste all'interno del file, pertanto offset e dimensione nel file sono campi azzerati;
    * il numero di pagine è definito dalla costante _PAGEVM_STACKPAGES_, pari a 18;
    * la dimensione occupata in memoria (parole intere) è legata direttamente al numero di pagine, secondo la costante _PAGE_SIZE_;
    * il puntatore al vnode è _NULL_, in quanto non è necessario mantenere tale informazione: essa è utilizzata, nel caso delle altre regioni, per caricare pagine in memoria dall'eseguibile, cosa che non avviene nel caso dello stack.
    
    Essa effettua anche l'allocazione della page table, per coerenza con il pattern seguito nella configurazione _DUMBVM_, in cui la funzione di preparazione non viene invocata sul segmento di stack.
- _seg_prepare_: utilizzata per allocare la page table relativa ai segmenti codice e dati, invocata una volta per ognuno dei segmenti, all'interno di _as_prepare_load_;
- _seg_copy_: effettua la copia in profondità di un segmento dato in un segmento destinazione, invocata in _as_copy_; si avvale dell'analoga funzione del modulo _pt_ per la copia della page table.

##### Operazioni di traduzione indirizzi
Si utilizzano le funzioni:
- _seg_get_paddr_: ottiene l'indirizzo fisico di una pagina di memoria, dato l'indirizzo virtuale che ha causato una TLB miss; è invocata da _vm_fault_, in seguito ad una TLB miss; utilizza direttamente la funzione analoga del modulo _pt_;
- _seg_add_pt_entry_: aggiunge alla page table la coppia (indirizzo virtuale, indirizzo fisico), passati come parametri, utilizzando l'analoga funzione del modulo _pt_; viene invocata in _vm_fault_, in seguito ad una TLB miss.

##### Loading (dinamico) di una pagina dall'eseguibile
Si utilizza la funzione _seg_load_page_, che costituisce buona parte della complessità di questo modulo e consente, di fatto, di implementare il loading dinamico delle pagine del programma dall'eseguibile. Dato il segmento associato, l'obiettivo è caricare in memoria la pagina associata ad un indirizzo virtuale (posto che essa non fosse né residente in memoria né _swapped_), ad un indirizzo fisico già opportunamente ricavato (e passato come parametro).

La pagina richiesta è rappresentata da un indice all'interno dell'eseguibile, calcolato a partire dal campo _base_vaddr_ del segmento; si possono presentare tre casi distinti:
- **prima pagina**: la pagina richiesta è la prima dell'eseguibile, il cui offset all'interno della pagina stessa potrebbe non essere nullo (ovvero, l'indirizzo virtuale di inizio dell'eseguibile potrebbe non essere _page aligned_) e, per semplicità, si mantiene tale offset anche a livello fisico, effettuando il caricamento della prima pagina anche se solo parzialmente occupata dall'eseguibile; ci sono due sottocasi possibili:
  * l'eseguibile termina nella pagina corrente;
  * l'eseguibile occupa anche altre pagine,
    
  tramite cui si determina quanti byte leggere dall'ELF file.
- **ultima pagina**: la pagina richiesta è l'ultima dell'eseguibile, di conseguenza il caricamento avviene ad un indirizzo _page aligned_, mentre l'offset all'interno dell'ELF file viene calcolato a partire dal numero di pagine totali e dall'offset all'interno della prima pagina; ci sono due sottocasi possibili:
  * l'eseguibile termina nella pagina corrente;
  * l'eseguibile termina in una pagina precedente, ma la pagina corrente è ancora occupata: ciò è dovuto al fatto che un file eseguibile ha una _filesize_ (rappresentata in _ps_t_ da _seg_size_bytes_) e una _memsize_ (rappresentata in _ps_t_ da _seg_size_words_) che potrebbero differire, con la prima minore o uguale alla seconda; in tal caso, l'area di memoria occupata (ma non valorizzata) deve essere azzerata,
  
  e, in particolare, nel secondo caso non si leggono byte dall'ELF file.
- **pagina intermedia**: il caricamento è analogo al caso precedente, a livello di offset nell'ELF file e di indirizzo fisico, ma si delineano tre sottocasi possibili, per quanto riguarda il numero di byte da leggere:
  * l'eseguibile termina in una pagina precedente;
  * l'eseguibile termina nella pagina corrente;
  * l'eseguibile occupa anche pagine successive,
  
  i quali sono gestiti analogamente alle due situazioni precedenti.

Dopo aver definito i tre parametri del caricamento, ovvero:
- _load_paddr_: indirizzo fisico in memoria a cui caricare la pagina;
- _load_len_bytes_: numero di byte da leggere;
- _elf_offset_: offset di inizio lettura all'interno del file eseguibile,

si azzera la regione di memoria deputata ad ospitare la pagina, per poi effettuare la lettura, seguendo il pattern dato dalle operazioni:
- _uio_kinit_ per effettuare il setup delle strutture dati _uio_ e _iovec_;
- _VOP_READ_ per effettuare l'operazione di lettura vera e propria.

Vengono inoltre effettuati:
- controlli sul risultato dell'operazione di lettura, per ritornare al chiamante eventuali errori;
- verifica sul parametro _load_len_bytes_, per fini statistici: se esso è nullo, si registra un page fault relativo ad una pagina azzerata, altrimenti riguarda una pagina da caricare dall'eseguibile (e dal disco).

##### Operazioni di swapping
Si utilizzano le funzioni:
- _seg_swap_out_: segna come _swapped out_ la pagina corrispondente all'indirizzo virtuale passato, mediante l'analoga funzione del modulo _pt_; è invocata da _getppage_user_ all'interno del modulo _coremap_, all'interno del quale si effettua fisicamente lo swap out del frame;
- _seg_swap_in_: effettua le operazioni di swap in del frame corrispondente all'indirizzo virtuale fornito, supponendo che la pagina fosse in stato di _swapped_:
    * ottiene l'offset nello swapfile, a partire dal contenuto della page table alla entry opportuna;
    * effettua fisicamente l'operazione di swap in del frame, utilizzando l'indirizzo fisico fornito;
    * utilizza l'analoga funzione del modulo _pt_ per inserire la corrispondenza (indirizzo virtuale, indirizzo fisico) nella entry opportuna.
 
### Page table
Come detto in precedenza, sono presenti tre page table per ogni processo, una per ognuno dei tre segmenti che li costituiscono; per questa ragione, non sono necessarie forme di locking, in quanto la page table non è una risorsa condivisa tra diversi processi, ma propria di un singolo processo.

#### Strutture dati
La struttura dati utilizzata per rappresentare la page table è definita in _pt.h_ ed è la seguente:
```C
typedef struct {
    unsigned long num_pages;
    vaddr_t base_vaddr;
    paddr_t *page_buffer;
} pt_t;
```
i cui campi hanno il significato seguente:
- _num_pages_: numero di pagine all'interno della page table;
- _base_vaddr_: indirizzo virtuale iniziale della page table, corrisponde con l'indirizzo virtuale di base del segmento e serve per calcolare la pagina di appartenenza di un indirizzo virtuale richiesto;
- _page_buffer_: vettore di indirizzi fisici delle pagine rappresentate, allocato dinamicamente in fase di creazione della page table.

Ogni entry della page table (ovvero ogni singolo elemento del buffer di pagine) può assumere i seguenti valori:
- _PT_EMPTY_ENTRY_ (0): poiché 0 non è un indirizzo fisico valido (è occupato dal kernel), viene utilizzato per indicare una entry vuota, ovvero una pagina non ancora caricata in memoria;
- _PT_SWAPPED_ENTRY_ (1) + swap offset: poiché 1 non è un indirizzo fisico valido (è occupato dal kernel), viene utilizzato per indicare una pagina di cui è stato effettuato swap out; dei 31 bit rimanenti, i meno significativi vengono utilizzati per rappresentare l'offset della pagina nello swapfile (esso ha dimensione 9 MB, pertanto sarebbero sufficienti 24 bit); il contenuto di queste entries non interferisce con gli indirizzi fisici validi in quanto e la CPU lavora con indirizzi multipli di 4;
- altri valori: in questo caso è presente un indirizzo fisico valido per la pagina, ovvero essa è presente in memoria e non è avvenuto un page fault.

Per poter ricavare in modo semplice l'indice della entry nel buffer, a partire da un indirizzo virtuale, il buffer è stato realizzato in modo che ogni entry occupi un'intera pagina: ciò occupa più memoria, ma semplifica notevolmente lo svolgimento di quasi tutte le operazioni effettuate sulla page table, in quanto ricavare l'indice a partire da un indirizzo virtuale è necessario in molte di esse.

#### Implementazione
Le funzioni di gestione della page table, come nel caso dei segmenti, si suddividono in diversi gruppi a seconda del compito che svolgono. I prototipi, definiti nel file _pt.h_, sono i seguenti:

&nbsp;

```C
pt_t *pt_create(unsigned long num_pages, vaddr_t base_address);
int pt_copy(pt_t *src, pt_t **dest);
paddr_t pt_get_entry(pt_t *pt, vaddr_t vaddr);
void pt_add_entry(pt_t *pt, vaddr_t vaddr, paddr_t paddr);
void pt_clear_content(pt_t *pt);
void pt_swap_out(pt_t *pt, off_t swapfile_offset, vaddr_t vaddr);
void pt_swap_in(pt_t *pt, vaddr_t vaddr, paddr_t paddr);
off_t pt_get_swap_offset(pt_t *pt, vaddr_t vaddr);
void pt_destroy(pt_t *pt);
```

##### Creazione e copia
Si utilizzano le funzioni:
- _pt_create_: alloca una nuova page table, definendo il numero di pagine e l'indirizzo virtuale di partenza, passati come parametri; il buffer utilizzato per la paginazione è allocato e azzerato, utilizzando la costante _PT_EMPTY_ENTRY_, in quanto inizialmente la page table è (concettualmente) vuota;
- _pt_copy_: copia il contenuto di una page table in una nuova, allocata all'interno della funzione; è utilizzato soltanto nel contesto della copia di un address space, invocata da _seg_copy_.

##### Cancellazione e distruzione
Si utilizzano le funzioni:
- _pt_clear_content_: effettua i side effects della cancellazione del contenuto della page table su swapfile e memoria fisica:
  * se una entry è _swapped_, la elimina dallo swapfile;
  * se una entry è in memoria, libera la memoria fisica,
  
  ed è utilizzata in fase di distruzione di un address space, invocata da _seg_destroy_;
- _pt_destroy_: rilascia le risorse di memoria detenute dalla page table, inclusi i buffer contenuti all'interno; come la precedente, è utilizzata in fase di distruzione di un address space ed è invocata da _seg_destroy_.

##### Operazioni di traduzione indirizzi
Si utilizzano le funzioni:
- _pt_get_entry_: ottiene l'indirizzo fisico di una pagina a partire dall'indirizzo virtuale; in particolare, ritorna le costanti:
  * _PT_EMPTY_ENTRY_ se l'indirizzo virtuale appartiene ad una pagina non memorizzata e non _swapped_;
  * _PT_SWAPPED_ENTRY_ se l'indirizzo virtuale appartiene ad una pagina _swapped_;
- _pt_add_entry_: inserisce un indirizzo fisico nella entry corrispondente all'indirizzo virtuale; entrambi sono passati come parametri e, in particolare, l'indirizzo fisico è opportunamente ricavato e fornito dal chiamante.

##### Operazioni di swapping
Si utilizzano le funzioni:
- _pt_swap_out_: segna come _swapped_ la entry corrispondente all'indirizzo virtuale fornito; utilizzando la costante _PT_SWAPPED_MASK_, memorizza anche l'offset nello swapfile della pagina a cui l'indirizzo virtuale appartiene;
- _pt_swap_in_: duale alla precedente, è di fatto solo un wrapper per _pt_add_entry_, in quanto necessita solo della scrittura di un nuovo indirizzo fisico in corrispondenza della entry relativa alla pagina a cui appartiene l'indirizzo virtuale dato;
- _pt_get_swap_offset_: dato un indirizzo virtuale, ricava l'offset nello swapfile della pagina a cui esso appartiene, attraverso i 31 bit più significativi della entry corrispondente; è utilizzata durante l'operazione di swap in, invocata da _seg_swap_in_. 

&nbsp;


### TLB
Il modulo _vm_tlb.c_ (con il relativo header file) contiene un'astrazione per la gestione e l'interfaccia con il TLB: non vengono aggiunte strutture dati, ma solo funzioni che svolgono funzione di wrapper (o poco più) rispetto alle funzioni di lettura/scrittura già esistenti, oltre alla gestione della politica di replacement.

#### Implementazione
Le funzioni implementate in questo modulo hanno i prototipi seguenti:
```C
void vm_tlb_invalidate_entries(void);
void vm_tlb_reset_current_victim(void);
uint64_t vm_tlb_peek_victim(void);
void vm_tlb_write(vaddr_t vaddr, paddr_t paddr, unsigned char dirty);
```

e svolgono i compiti seguenti:
- _vm_tlb_invalidate_entries_: invalida tutte le entries del TLB, utilizzando le apposite maschere definite in _mips/tlb.h_; è invocata da _as_activate_, ovvero all'inizio del processo e ad ogni context switching;
- _vm_tlb_reset_current_victim_: resetta a 0 la posizione della vittima dell'algoritmo round-robin, usato per gestire il replacement; è invocata da _vm_bootstrap_, durante il bootstrap del sistema operativo;
- _vm_tlb_peek_victim_: effettua una lettura nel TLB (mediante la funzione _tlb_read_), della entry corrispondente alla vittima corrente; è utilizzata per verificare che la vittima corrente sia una entry valida o meno, per fini statistici, in seguito a TLB miss;
- _vm_tlb_write_: scrive la coppia (_vaddr_, _paddr_), all'interno della entry corrispondente alla vittima corrente (che a sua volta può essere una entry valida o meno), utilizzando la funzione _tlb_write_; la posizione della vittima è ricavata attraverso la funzione _vm_tlb_get_victim_round_robin_, che incrementa di un'unità (in modo circolare) l'indice della vittima, per poi ritornare quella corrente, eseguendo di fatto l'algoritmo di replacement; è invocata in seguito ad una TLB miss, in assenza di altri errori. Inoltre, se l'indirizzo virtuale appartiene ad una pagina con permesso di scrittura, viene settato il _dirty bit_, il quale (nel TLB di os161) indica se la entry corrispondente contiene l'indirizzo di una pagina _writable_.

Le funzioni _tlb_read_ e _tlb_write_ sono implementate direttamente in linguaggio assembly e i loro prototipi sono definiti nel file _mips/tlb.h_.

### Statistiche
Il modulo _vmstats.c_ (con il relativo header file) contiene le strutture dati e le funzioni per la gestione delle statistiche relative al gestore della memoria.

#### Strutture dati
Le strutture dati sono definite in _vmstats.c_, per poter implementare _information hiding_, accedendovi soltanto con funzioni esposte all'esterno. Esse sono:
```C
bool vmstats_active = false;
struct spinlock vmstats_lock;

unsigned int vmstats_counts[VMSTATS_NUM];

static const char *vmstats_names[VMSTATS_NUM] = {
    "TLB faults",                 /* TLB misses */
    "TLB faults with free",       /* TLB misses with no replacement */
    "TLB faults with replace",    /* TLB misses with replacement */
    "TLB invalidations",          /* TLB invalidations (number of times) */
    "TLB reloads",                /* TLB misses for pages stored in memory */
    "Page faults (zeroed)",       /* TLB misses requiring zero-filled page */
    "Page faults (disk)",         /* TLB misses requiring load from disk */
    "Page faults from ELF",       /* Page faults requiring load from ELF */
    "Page faults from swapfile",  /* Page faults requiring load from swapfile */
    "Swapfile writes"             /* Page faults requiring write on swapfile */
};
```
nell'ordine:
- _vmstats_active_: flag per indicare se il modulo è attivo (ovvero i contatori sono stati inizializzati opportunamente);
- _vmstats_lock_: spinlock per l'accesso in mutua esclusione, necessario in quanto tale modulo (con le sue strutture dati) è condiviso a tutti i processi e richiede che gli incrementi siano indipendenti;
- _vmstats_counts_: vettore di contatori, uno per ogni statistica;
- _vmstats_names_: vettore di stringhe, contenenti i nomi delle statistiche da collezionare, utile in fase di stampa.

Nell'header file (_vmstats.h_) sono invece definiti:
```C
#define VMSTATS_NUM 10

enum vmstats_counters {
    VMSTAT_TLB_MISS,
    VMSTAT_TLB_MISS_FREE,
    VMSTAT_TLB_MISS_REPLACE,
    VMSTAT_TLB_INVALIDATION,
    VMSTAT_TLB_RELOAD,
    VMSTAT_PAGE_FAULT_ZERO,
    VMSTAT_PAGE_FAULT_DISK,
    VMSTAT_PAGE_FAULT_ELF,
    VMSTAT_PAGE_FAULT_SWAPFILE,
    VMSTAT_SWAPFILE_WRITE
};
```
ovvero:
- _VMSTATS_NUM_: il numero di statistiche da collezionare;
- _vmstats_counters_: i nomi delle statistiche da collezionare, utilizzati come indice in _vmstats_counts_ e _vmstats_names_, esposti all'esterno per poter essere utilizzati nell'invocazione della funzione di incremento

#### Implementazione
Le funzioni implementate in questo modulo hanno i prototipi seguenti:
```C
void vmstats_init(void);    
void vmstats_increment(unsigned int stat_index);
void vmstats_show(void);
```
Esse svolgono i compiti seguenti:
- _vmstats_init_: inizializza il flag _vmstats_active_, dopo aver azzerato tutti i contatori in _vmstats_counts_; viene invocata al bootstrap del gestore della memoria virtuale;
- _vmstats_increment_: incrementa di un'unità la statistica associata all'indice fornito come parametro, effettuando il conteggio;
- _vmstats_show_: stampa, per ogni statistica, il valore di conteggio associato, mostrando eventuali messaggi di warning qualora le relazioni presenti tra le statistiche non fossero rispettate; viene invocata allo shutdown del gestore della memoria virtuale.

Ogni operazione effettuata all'interno delle funzioni di inizializzazione e incremento è protetta da spinlock, in quanto richiede accesso in mutua esclusione, poiché si realizzano scritture su dati condivisi; la funzione di stampa effettua solo letture, pertanto non richiede l'utilizzo di forme di locking.

### Coremap
La coremap è una componente fondamentale per la gestione della memoria fisica all'interno del sistema di memoria virtuale. Questa struttura dati tiene traccia dello stato di ogni pagina in memoria fisica, consentendo al sistema di sapere quali pagine sono attualmente in uso, quali sono libere e quali devono essere sostituite o recuperate dal disco. La coremap gestisce sia le pagine utilizzate dal kernel che quelle utilizzate dai processi utente, facilitando la gestione dinamica della memoria in base alle esigenze del sistema.
#### Strutture dati
La struttura dati utilizzata per la gestione della coremap è definita in coremap.h ed è la seguente:
```C
struct coremap_entry {
    coremap_entry_state entry_type; 
    unsigned long allocation_size;        
    
    unsigned long previous_allocated;   
    unsigned long next_allocated;   
    
    vaddr_t virtual_address;                  
    addrspace_t *address_space;                
};
```
Questa struttura serve a rappresentare lo stato e le proprietà di una singola pagina di memoria fisica. Ogni campo ha un ruolo specifico:
- _entry_type_: indica lo stato attuale della pagina, utilizzando la enum _coremap_entry_state_. Può assumere lo stato:
  * COREMAP_BUSY_KERNEL: la pagina è allocata per l'uso del kernel.
  * COREMAP_BUSY_USER: la pagina è allocata per l'uso utente.
  * COREMAP_UNTRACKED: la pagina non è ancora gestita dalla coremap.
  * COREMAP_FREED: la pagina è stata liberata e può essere riutilizzata.

- _allocation_size_: specifica la dimensione dell'allocazione, ovvero il numero di pagine contigue allocate. Questo è particolarmente rilevante per le allocazioni del kernel, che possono richiedere blocchi di pagine contigue.

- _previous_allocated_ e _next_allocated_:  fungono da puntatori per una lista collegata delle pagine allocate. Vengono utilizzati per implementare una strategia FIFO (First-In-First-Out) per la sostituzione delle pagine, facilitando il tracking delle pagine in ordine di allocazione.

- _virtual_address_:  memorizza l'indirizzo virtuale associato alla pagina. È particolarmente importante per le pagine utente, dove il sistema deve mappare l'indirizzo virtuale dell'utente alla pagina fisica corrispondente.

- _address_space_:  punta allo spazio di indirizzamento (address space) a cui è allocata la pagina. È utilizzato per identificare quale processo utente sta utilizzando la pagina.

#### Implementazione
L'implementazione della coremap è fondamentale per la gestione della memoria all'interno del sistema operativo. I seguenti prototipi sono definiti per gestire l'inizializzazione, l'allocazione, la deallocazione e lo shutdown della coremap:

&nbsp;

&nbsp;


```C
void coremap_init(void);            
void coremap_shutdown(void);        
vaddr_t alloc_kpages(unsigned npages); 
void free_kpages(vaddr_t addr);     
paddr_t alloc_user_page(vaddr_t vaddr); 
void free_user_page(paddr_t paddr); 
```

##### Inizializzazione e Terminazione

- _coremap_init()_: funzione che inizializza la coremap, allocando la memoria necessaria per gestire tutte le pagine di memoria fisica disponibili. Imposta ogni entry inizialmente come COREMAP_UNTRACKED, indicando che le pagine non sono ancora gestite.

- _coremap_shutdown()_: funzione responsabile dell'arresto e della pulizia della coremap, liberando la memoria allocata e disattivando la coremap quando il sistema non ne ha più bisogno.

##### Allocazione e Deallocazione pagine - Kernel

- _alloc_kpages(unsigned npages)_: funzione che gestisce l'allocazione delle pagine per il kernel. Tenta di allocare _npages_ contigue, restituendo l'indirizzo virtuale del primo blocco. Se non ci sono pagine libere sufficienti, il sistema tenta di "rubare" memoria richiamando la funzione _ram_stealmem()_, fornita di default da os161 in ram.c, la quale ritorna l'indirizzo fisico di un frame che non è stato ancora utilizzato.

- _free_kpages(vaddr_t addr)_: funzione che libera le pagine allocate al kernel, segnandole come COREMAP_FREED nella coremap, rendendole disponibili per future allocazioni.

##### Allocazione e Deallocazione pagine - Processi utente

- _alloc_user_page(vaddr_t vaddr)_: funzione che gestisce l'allocazione delle pagine per i processi utente. Cerca prima di utilizzare pagine libere e, se necessario, sostituisce una pagina esistente usando una strategia di sostituzione FIFO. Se una pagina viene sostituita, la funzione interagisce con lo swapfile per gestire il trasferimento della pagina vittima al disco. Inoltre è fondamentale, dal punto di vista implementativo, identificare il segmento che contiene la pagina selezionata come vittima. Questo passaggio è essenziale per poter marcare come "swapped" l'entry della corretta page table corrispondente all'indirizzo virtuale e per salvare l'offset dello swapfile dove la pagina è stata memorizzata. La ricerca del segmento viene effettuata tramite la funzione 
`ps_t *as_find_segment_coarse(addrspace_t *as, vaddr_t vaddr)` definita in _addrspace.h_.

- _free_user_page(paddr_t paddr)_: funzione che libera le pagine allocate ai processi utente, rimuovendo la pagina dalla coda di allocazione e segnandola come COREMAP_FREED nella coremap.

### Swapfile
Lo swapfile è un componente essenziale per estendere la capacità di memoria fisica del sistema, permettendo al sistema operativo di gestire più processi di quanti possano essere contenuti nella memoria fisica disponibile. Quando la memoria RAM è piena, lo swapfile permette di spostare temporaneamente le pagine su disco, liberando memoria per altri processi.
#### Implementazione
L'implementazione dello swapfile prevede diverse funzioni per la gestione dello spazio di swap e il trasferimento delle pagine tra memoria fisica e disco. Lo swapfile è limitato a 9 MB (dimensione definita in _swapfile.h_) e se a tempo di esecuzione viene richiesto uno spazio di swap maggiore, il sistema panica indicando la violazione. I prototipi delle funzioni principali sono:
```C
int swap_init(void);
int swap_out(paddr_t page_paddr, off_t *ret_offset);
int swap_in(paddr_t page_paddr, off_t swap_offset);
void swap_free(off_t swap_offset);
void swap_shutdown(void);
```

##### Inizializzazione

- _swap_init()_: funzione che inizializza lo swapfile e la bitmap utile a tracciare lo stato di utilizzo delle pagine nel file di swap.

##### Operazioni di swapping

- _swap_out(paddr_t page_paddr, off_t *ret_offset)_: funzione che scrive una pagina dalla memoria fisica al file di swap. Il parametro _page_paddr_ indica l'indirizzo fisico della pagina da spostare su disco. La funzione restituisce l'offset nel file di swap dove la pagina è stata memorizzata, permettendo di recuperarla in seguito.

- _swap_in(paddr_t page_paddr, off_t swap_offset)_: funzione che legge una pagina dal file di swap e la ripristina nella memoria fisica. Il parametro _swap_offset_ indica l'offset nel file di swap da cui recuperare la pagina.

##### Pulizia e Terminazione

- _swap_free(off_t swap_offset)_: funzione che libera lo spazio nel file di swap associato ad una pagina che non è più necessaria. Il parametro _swap_offset_ specifica l'offset della pagina nel file di swap da liberare.

- _swap_shutdown()_: funzione che chiude e libera le risorse associate allo swapfile quando il sistema non ne ha più bisogno. Chiude il file di swap e rilascia la memoria utilizzata dalla bitmap.

### Modifiche ad altri file
Di seguito si riportano le modifiche (minoritarie, ma necessarie) effettuate ad altri file del kernel di os161, già esistenti nella versione di partenza.

#### trap.c
All'interno della funzione _kill_curthread_, in caso di:
- TLB miss in read/write;
- TLB hit in caso di richiesta di write (memory store) su segmento di memoria read-only;

viene eseguita una stampa di errore, seguita da una chiamata alla system call _sys__exit_, per effettuare la terminazione _graceful_ del processo, liberando le risorse allocate; ciò avviene di solito in seguito alla restituzione di un valore non nullo da parte della funzione _vm_fault_.

In questo modo, è possibile evitare un _panic_ del sistema operativo, qualora si verifichi un errore di questo tipo, permettendo sia l'esecuzione di ulteriori test (o la ripetizione dello stesso); inoltre, ciò permette di terminare correttamente il sistema operativo (con il comando _q_), tracciando le statistiche per il test _faulter_.

Tale modifica è valida solo quando l'opzione condizionale _OPT_PAGING_ è abilitata.

#### runprogram.c
In questa implementazione, è stata aggiunta una modifica con un flag condizionale (utilizzando #if !OPT_PAGING) per determinare se il file rimane aperto o viene chiuso subito dopo il caricamento dell'eseguibile. Se l'opzione di paging (OPT_PAGING) è disabilitata, il file viene chiuso immediatamente. Altrimenti, il file rimane aperto per essere chiuso successivamente durante la distruzione dello spazio di indirizzamento chiamando la _as_destroy_. Questa modifica è stata introdotta per supportare il loading dinamico, che necessita dell'accesso continuo al file eseguibile durante l'esecuzione del programma.

&nbsp;

#### loadelf.c
In questa implementazione, il codice è stato modificato per supportare il loading dinamico, che consente di caricare pagine dell'eseguibile in memoria solo quando è abilitata l'opzione condizionale OPT_PAGING. In tal caso, _as_define_region_ viene chiamata con parametri aggiuntivi che includono l'offset del file, la dimensione in memoria, la dimensione del file e il puntatore al file. Questo consente alla funzione di gestire le regioni di memoria in modo da supportare la paginazione a richiesta.

La funzione _as_prepare_load_ viene chiamata per preparare il caricamento del programma nello spazio di indirizzamento. Tuttavia, se la paginazione a richiesta è attiva, il caricamento effettivo delle pagine dei segmenti in _load_segment_ non viene eseguito in questa fase.

#### main.c
All'interno della funzione _boot_ è stata inserita la chiamata a _vm_shutdown_, per effettuare la terminazione del gestore della memoria virtuale permettendo, tra l'altro, la visualizzazione delle statistiche sul terminale; tale invocazione avviene solo quando l'opzione condizionale _OPT_PAGING_ è abilitata.

## Test
Per verificare il corretto funzionamento del sistema, abbiamo utilizzato i test già presenti all'interno di os161, scegliendo quelli adatti per ciò che è stato sviluppato:
- _palin_: effettua un semplice controllo su una stringa di 8000 caratteri, senza stressare la VM; non provoca replacements del TLB né swap in di pagine;
- _matmult_: effettua un prodotto matriciale (controllando il risultato ottenuto con quello atteso), occupando molto spazio in memoria e stressando maggiormente la VM rispetto al precedente;
- _sort_: ordina un array di grandi dimensioni usando l'algoritmo _quick sort_;
- _zero_: verifica che le aree di memoria da azzerare in allocazione siano correttamente azzerate (si ignora il controllo effettuato sulla syscall _sbrk_);
- _faulter_: verifica che l'accesso illegale ad un'area di memoria produca l'interruzione del programma;
- _ctest_: effettua l'attraversamento di una linked list;
- _huge_: alloca e manipola un array di grandi dimensioni.

Per assicurarci che le funzioni basilari del kernel fossero già correttamente implementate, abbiamo eseguito i kernel tests seguenti:
- _at_: gestione degli array;
- _at2_: come il precedente, ma con array di grandi dimensioni;
- _bt_: gestione della bitmap;
- _km1_: verifica della kmalloc;
- _km2_: come il precedente, ma in condizioni di stress.

Di seguito si riportano le statistiche registrate per ogni test: ognuno è stato eseguito una volta sola, per poi effettuare lo shutdown del sistema.

&nbsp;

&nbsp;

&nbsp;

&nbsp;

&nbsp;

&nbsp;

&nbsp;


| Nome test | TLB faults | TLB faults (free) | TLB faults (replace) | TLB invalidations | TLB reloads | Page faults (zeroed) | Page faults (disk) | Page faults (ELF) | Page faults (swapfile) | Swapfile writes |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
|palin|13889|13889|0|7789|13884|1|4|4|0|0|
|matmult|4342|4323|19|1222|3534|380|428|3|425|733|
|sort|7052|7014|38|3144|5316|289|1447|4|1443|1661|
|zero|143|143|0|139|137|3|3|3|0|0|
|faulter|61|61|0|132|58|2|1|1|0|0|
|ctest|248591|248579|12|249633|123627|257|124707|3|124704|124889|
|huge|7459|7441|18|6752|3880|512|3067|3|3064|3506|
