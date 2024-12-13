#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Macros pour le partitionnement des données entre les processus
#define MIN(a,b) ((a)<(b)?(a):(b))
#define BLOCK_LOW(id, p, n) ((id) * (n) / (p))
#define BLOCK_HIGH(id, p, n) (MIN(BLOCK_LOW((id)+1,p,n)-1, n))
#define BLOCK_SIZE(id, p, n) (BLOCK_LOW((id)+1, p, n)-BLOCK_LOW(id, p, n))
#define BLOCK_OWNER(index,p,n) ((p*(index+1)-1)/(n)) // Identifie quel processus possède un indice donné

int main (int argc, char *argv[]) {
    int id, p;                   // id : Identifiant du processus, p : Nombre total de processus
    int n;                       // Limite supérieure des nombres à tester
    int low_value, high_value;   // Valeurs locales minimale et maximale pour chaque processus
    int size;                    // Taille du bloc de nombres à traiter par un processus
    int proc0_size;              // Taille du bloc de données pour le processus 0
    int i, index, prime, first;  // Variables de contrôle pour l'algorithme
    int count, global_count;     // Nombre local et global de couples sexy trouvés
    char *marked;                // Tableau pour marquer les multiples (0 = premier, 1 = non-premier)
    double elapsed_time;         // Temps écoulé pour l'exécution

    // Initialisation de MPI
    MPI_Init(&argc, &argv);
    MPI_Barrier(MPI_COMM_WORLD); // Synchronise tous les processus
    elapsed_time = -MPI_Wtime(); // Commence la mesure du temps
    MPI_Comm_rank(MPI_COMM_WORLD, &id); // Récupère l'ID du processus courant
    MPI_Comm_size(MPI_COMM_WORLD, &p);  // Récupère le nombre total de processus

    // Vérifie que l'utilisateur a donné un argument
    if (argc != 2) {
        if (!id) printf("Command line: %s <n>\n", argv[0]);
        MPI_Finalize();
        exit(1);         
    }

    n = atoi(argv[1]);
    // Calcule les limites des données locales pour chaque processus
    low_value = 2 + BLOCK_LOW(id, p, n-1);
    high_value = 2 + BLOCK_HIGH(id, p, n-1);
    size = BLOCK_SIZE(id, p, n-1);
    proc0_size = (n-1) / p;

    // Vérifie si le nombre de processus est trop élevé
    if ((2 + proc0_size) < (int)sqrt((double)n)) {
        if (!id) printf("Too many processes\n");
        MPI_Finalize();
        exit(1);
    }

    marked = (char *)malloc(size);
    if (marked == NULL) {
        printf("Cannot allocate enough memory\n");
        MPI_Finalize();
        exit(1);
    }
    for (i = 0; i < size; i++) marked[i] = 0;
    
    if (!id) index = 0;
    prime = 2;
    // Recherche des nombres premiers
    do {
        if (prime * prime > low_value){
            first = prime * prime - low_value;
        }
        else {
            if (!(low_value % prime)) first = 0;
            else first = prime - (low_value % prime);
        }

        for (i = first; i < size; i += prime) marked[i] = 1;

        if (!id) {
            while (marked[++index]);
            prime = index + 2;
        }

        // Diffuse le nouveau k à tous les processus
        MPI_Bcast(&prime, 1, MPI_INT, 0, MPI_COMM_WORLD);
    } while (prime * prime <= n);

    count = 0;
    // Compte le nombre de couples sexys locaux
    for (i = 0; i < size; i++) {
        
        if (id !=0 && (low_value + i >= 6 + 2) && i < 6){
            int received_number;
            MPI_Recv(&received_number, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (received_number != 0 && !marked[received_number - low_value]) {
                count++;
                //printf("Process : %d;  %d-%d\n", id, received_number, received_number-6);
            }
        }

        int target_number = low_value + i + 6;
        if (target_number <= high_value) {
            if (!marked[i] && !marked[i + 6]) count++;
        }

        else if (target_number <= n){
            int target_process = BLOCK_OWNER(target_number - 2, p, n-1);
            if (!marked[i]) {
                MPI_Send(&target_number, 1, MPI_INT, target_process, 0, MPI_COMM_WORLD);
                //printf("Process %d sends target_number %d to process %d\n", id, target_number, target_process);
            }
            else {
                int msg = 0;
                MPI_Send(&msg, 1, MPI_INT, target_process, 0, MPI_COMM_WORLD);
            }
        }
    }

    // Réduit les résultats locaux pour obtenir le nombre global de couples sexys
    MPI_Reduce(&count, &global_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    // Fin de la mesure du temps
    elapsed_time += MPI_Wtime();

    // Le processus 0 affiche les résultats
    if (!id) {
        printf("%d sexy couples until %d\n", global_count, n);
        printf("Total elapsed time: %10.6f\n", elapsed_time);
    }

    free(marked);
    MPI_Finalize();
    return 0;
}
