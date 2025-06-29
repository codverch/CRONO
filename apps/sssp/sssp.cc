/*
    Distributed Under the MIT license
    Uses the Range based of Bellman-Ford/Dijkstra Algorithm to find shortest path distances
    For more info, see Yen's Optimization for the Bellman-Ford Algorithm, or the Pannotia Benchmark Suite.
    Programs by Masab Ahmad (UConn)
*/

// Usage: ./sssp 2 p2p-Gnutella30.txt p2p-Gnutella31.txt 

#include <cstdio>
#include <cstdlib>
#include <pthread.h>
//#include "carbon_user.h"    /*For the Graphite Simulator*/
#include <time.h>
#include <sys/timeb.h>

#define MAX            100000000
#define INT_MAX        100000000
#define BILLION 1E9

//Thread Argument Structure
typedef struct
{
   int*      local_min;
   int*      global_min;
   int*      Q;
   int*      D;
   int**     W;
   int**     W_index;
   int*      exist;        
   int*      d_count;
   int       tid;
   int       P;
   int       N;
   int       DEG;
   int       input_id;     
   pthread_barrier_t* barrier;
} thread_arg_t;

//Function Initializers
int initialize_single_source(int* D, int* Q, int source, int N);
void relax(int u, int i, volatile int* D, int** W, int** W_index, int N);
int get_local_min(volatile int* Q, volatile int* D, int start, int stop, int N, int** W_index, int** W, int u);
void init_weights(int N, int DEG, int** W, int** W_index);

//Global Variables
int min = INT_MAX;
int min_index = 0;
pthread_mutex_t lock;
pthread_mutex_t locks[2097152]; //change the number of locks to approx or greater N
int u = -1;
int local_min_buffer[1024];
int global_min_buffer;
int terminate = 0;
int range=1;       //starting range
int old_range =1;
int difference=0;
int pid=0;
int *exist;
int *id;
int P_max=256;     //Heuristic parameter. A lower value will result in incorrect distances
thread_arg_t thread_arg[1024];
pthread_t   thread_handle[1024];

//Primary Parallel Function
void* do_work(void* args)
{
   volatile thread_arg_t* arg = (thread_arg_t*) args;

   int tid                  = arg->tid;
   int P                    = arg->P;
   int* D                   = arg->D;        // Now points to correct D or D2
   int** W                  = arg->W;        // Now points to correct W or W2  
   int** W_index            = arg->W_index;  // Now points to correct W_index or W_index2
   int* exist_arr           = arg->exist;    //Get the correct exist array
   const int N              = arg->N;
   const int DEG            = arg->DEG;
   int v = 0;

   int cntr = 0;
   int start = 0;
   int stop = 1;
   int neighbor=0;

   //For precision dynamic work allocation
   double P_d = P;
   double range_d = 1.0;
   double tid_d = tid;

   pthread_barrier_wait(arg->barrier);

   while(terminate==0){
      while(terminate==0)
      {
         for(v=start;v<stop;v++)
         {

            if(exist_arr[v]==0)
               continue;

            for(int i = 0; i < DEG; i++)
            {
               if(v<N)
                  neighbor = W_index[v][i];

               if(neighbor>=N)
                  break;
               if((D[W_index[v][i]] > (D[v] + W[v][i]))) {     //Uncomment for test and test and set
               
               pthread_mutex_lock(&locks[neighbor]);
               //if(v>=N)
               //	terminate=1;
               //relax
               if((D[W_index[v][i]] > (D[v] + W[v][i])))        //relax, update distance
                  D[W_index[v][i]] = D[v] + W[v][i];
               //Q[v]=0;
               pthread_mutex_unlock(&locks[neighbor]);
							}                                               //Uncomment for test and test and set
            }
         }

         pthread_barrier_wait(arg->barrier);

         if(tid==0)
         {
            //range heuristic here
            range = range*DEG; //change this for range heuristic e.g. range = range+DEG;

            if(range>=N)
               range=N;    

         }

         pthread_barrier_wait(arg->barrier);

         range_d = range;
         double start_d = (range_d/P_d) * tid_d;
         double stop_d = (range_d/P_d) * (tid_d+1.0);
         start = start_d;//tid * (range/P);
         stop = stop_d;//(tid+1) * (range/P);

         if(stop>range)
            stop=range;	

         //{ pthread_mutex_lock(&lock);
         if(start==N || v>N-1)
            terminate=1;
         //} pthread_mutex_unlock(&lock);

         pthread_barrier_wait(arg->barrier);

         //printf("\n TID:%d   start:%d stop:%d terminate:%d",tid,start,stop,terminate);
      }
      pthread_barrier_wait(arg->barrier);
      if(tid==0)
      {
         cntr++;
         if(cntr<P_max)
         {
            terminate=0;
            //old_range=1;
            range=1;
         }
      }
      start=0;
      stop=1;
      pthread_barrier_wait(arg->barrier);
   }
   return NULL;
}

// Create a dotty graph named 'fn'.
void make_dot_graph(int **W,int **W_index,int *exist,int *D,int N,int DEG,const char *fn)
{
   FILE *of = fopen(fn,"w");
   if (!of) {
      printf ("Unable to open output file %s.\n",fn);
      exit (EXIT_FAILURE);
   }

   fprintf (of,"digraph D {\n"
         "  rankdir=LR\n"
         "  size=\"4,3\"\n"
         "  ratio=\"fill\"\n"
         "  edge[style=\"bold\"]\n"
         "  node[shape=\"circle\",style=\"filled\"]\n");

   // Write out all edges.
   for (int i = 0; i != N; ++i) {
      if (exist[i]) {
         for (int j = 0; j != DEG; ++j) {
            if (W_index[i][j] != INT_MAX) {
               fprintf (of,"%d -> %d [label=\"%d\"]\n",i,W_index[i][j],W[i][j]);
            }
         }
      }
   }

# ifdef DISTANCE_LABELS
   // We label the vertices with a distance, if there is one.
   fprintf (of,"0 [fillcolor=\"red\"]\n");
   for (int i = 0; i != N; ++i) {
      if (D[i] != INT_MAX) {
         fprintf (of,"%d [label=\"%d (%d)\"]\n",i,i,D[i]);
      }
   }
# endif

   fprintf (of,"}\n");

   fclose (of);
}

int main(int argc, char** argv)
{
   if (argc < 4) {
   printf ("Usage:  %s <thread-count> <input-file1> <input-file2>\n",argv[0]);
   return 1;
   }

   int N = 0;
   int DEG = 0;
   FILE *file0 = NULL;
   FILE *file1 = NULL;  // Add second file
   

   const int select = 1;              // Always file input mode
   const int P = atoi(argv[1]);       // Thread count is now first argument


   if (!P) {
      printf ("Error:  Thread count must be a valid integer greater than 0.");
      return 1;
   }

  
      const char *filename1 = argv[2];
      const char *filename2 = argv[3]; 

      file0 = fopen(filename1,"r");
      if (!file0) {
         printf ("Error:  Unable to open input file '%s'\n",filename1);
         return 1;
      }

      file1  = fopen(filename2,"r");
      if (!file1) {
         printf ("Error:  Unable to open input file '%s'\n",filename2);
         fclose(file0);  // Clean up first file
         return 1;
      }

      N = 2000000;  //can be read from file if needed, this is a default upper limit
      DEG = 500;     //also can be reda from file if needed, upper limit here again
   

   char c;
   int number0;
   int number1;
   int previous_node = -1;
   int inter = -1;

   if (DEG > N)
   {
      fprintf(stderr, "Degree of graph cannot be grater than number of Vertices\n");
      exit(EXIT_FAILURE);
   }

   int* D;
   int* Q;
   int* D2;     
   int* Q2;     
   int* exist2; 
   int* id2;


   if (posix_memalign((void**) &D2, 64, N * sizeof(int))) 
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if( posix_memalign((void**) &Q2, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if( posix_memalign((void**) &exist2, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &id2, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }

   if (posix_memalign((void**) &D, 64, N * sizeof(int))) 
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   
   if( posix_memalign((void**) &Q, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if( posix_memalign((void**) &exist, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   if(posix_memalign((void**) &id, 64, N * sizeof(int)))
   {
      fprintf(stderr, "Allocation of memory failed\n");
      exit(EXIT_FAILURE);
   }
   int d_count = N;
   pthread_barrier_t barrier;

   int** W = (int**) malloc(N*sizeof(int*));
   int** W_index = (int**) malloc(N*sizeof(int*));
   for(int i = 0; i < N; i++)
   {
      int ret = posix_memalign((void**) &W[i], 64, DEG*sizeof(int));
      int re1 = posix_memalign((void**) &W_index[i], 64, DEG*sizeof(int));
      if (ret != 0 || re1!=0)
      {
         fprintf(stderr, "Could not allocate memory\n");
         exit(EXIT_FAILURE);
      }
   }

   int** W2 = (int**) malloc(N*sizeof(int*));
   int** W_index2 = (int**) malloc(N*sizeof(int*));
   for(int i = 0; i < N; i++)
   {
      int ret = posix_memalign((void**) &W2[i], 64, DEG*sizeof(int));
      int re1 = posix_memalign((void**) &W_index2[i], 64, DEG*sizeof(int));
      if (ret != 0 || re1!=0)
      {
         fprintf(stderr, "Could not allocate memory\n");
         exit(EXIT_FAILURE);
      }
   }

   for(int i=0;i<N;i++)
   {
      for(int j=0;j<DEG;j++)
      {
         W[i][j] = INT_MAX;
         W_index[i][j] = INT_MAX;
         W2[i][j] = INT_MAX;        
         W_index2[i][j] = INT_MAX; 
      }
      exist[i]=0;
      exist2[i]=0;  
      id[0] = 0;
      id2[0] = 0;   
   }

   // Parse first input file
      int lines_to_check1 = 0;
      for(c=getc(file0); c!=EOF; c=getc(file0))
      {
         if(c=='\n')
            lines_to_check1++;

         if(lines_to_check1>3)
         {   
            int f0 = fscanf(file0, "%d %d", &number0,&number1);
            if(f0 != 2 && f0 != EOF)
            {
               printf ("Error: Read %d values, expected 2. Parsing failed.\n",f0);
               exit (EXIT_FAILURE);
            }

            if (number0 >= N) {
               printf ("Error:  Node %d exceeds maximum graph size of %d.\n",number0,N);
               exit (EXIT_FAILURE);
            }

            exist[number0] = 1; exist[number1] = 1;
            id[number0] = number0;
            if(number0==previous_node) {
               inter++;
            } else {
               inter=0;
            }

            if (inter >= DEG) {
               printf ("Error:  Node %d, maximum degree of %d exceeded.\n",number0,DEG);
               exit (EXIT_FAILURE);
            }

            bool exists = false;
            for (int i = 0; i != inter; ++i) {
               if (W_index[number0][i] == number1) {
                  exists = true;
                  break;
               }
            }

            if (!exists) {
               W[number0][inter] = inter+1;
               W_index[number0][inter] = number1;
               previous_node = number0;
            }
         }   
      }

      // Reset variables for second file
      previous_node = -1;
      inter = -1;
      int lines_to_check2 = 0;

      // Parse second input file  
      for(c=getc(file1); c!=EOF; c=getc(file1))
      {
         if(c=='\n')
            lines_to_check2++;

         if(lines_to_check2>3)
         {   
            int f0 = fscanf(file1, "%d %d", &number0,&number1);
            if(f0 != 2 && f0 != EOF)
            {
               printf ("Error: Read %d values, expected 2. Parsing failed.\n",f0);
               exit (EXIT_FAILURE);
            }

            if (number0 >= N) {
               printf ("Error:  Node %d exceeds maximum graph size of %d.\n",number0,N);
               exit (EXIT_FAILURE);
            }

            exist2[number0] = 1; exist2[number1] = 1;  // Use second arrays
            id2[number0] = number0;
            if(number0==previous_node) {
               inter++;
            } else {
               inter=0;
            }

            if (inter >= DEG) {
               printf ("Error:  Node %d, maximum degree of %d exceeded.\n",number0,DEG);
               exit (EXIT_FAILURE);
            }

            bool exists = false;
            for (int i = 0; i != inter; ++i) {
               if (W_index2[number0][i] == number1) {  // Use second arrays
                  exists = true;
                  break;
               }
            }

            if (!exists) {
               W2[number0][inter] = inter+1;           // Use second arrays
               W_index2[number0][inter] = number1;     // Use second arrays
               previous_node = number0;
            }
         }   
      }

   

   //Initialize data structures
   initialize_single_source(D, Q, 0, N);
   initialize_single_source(D2, Q2, 0, N);

   // Calculate and print maximum degree
   int max_degree = 0;
   int active_vertices = 0;
   
   for(int i = 0; i < N; i++) {
      if(exist[i] == 1) {
         active_vertices++;
         int vertex_degree = 0;
         for(int j = 0; j < DEG; j++) {
            // For file input (select=1): check != INT_MAX
            // For generated graphs (select=0): check != -1 and != INT_MAX
            if(select == 1) {
               if(W_index[i][j] != INT_MAX && W_index[i][j] < N) {
                  vertex_degree++;
               }
            } else {
               if(W_index[i][j] != INT_MAX && W_index[i][j] != -1 && W_index[i][j] < N) {
                  vertex_degree++;
               }
            }
         }
         if(vertex_degree > max_degree) {
            max_degree = vertex_degree;
         }
      }
   }
   
   printf("Maximum degree in the graph: %d\n", max_degree);
   printf("Active vertices: %d\n", active_vertices);

   printf("Initialization phase completed successfully.\n");

   //Synchronization Variables
   pthread_barrier_init(&barrier, NULL, P);
   pthread_mutex_init(&lock, NULL);
   for(int i=0; i<N; i++)
   {
      pthread_mutex_init(&locks[i], NULL);
   }

   //Thread Arguments
   for(int j = 0; j < P; j++) {
      thread_arg[j].local_min  = local_min_buffer;
      thread_arg[j].global_min = &global_min_buffer;
      thread_arg[j].tid        = j;
      thread_arg[j].P          = P;
      thread_arg[j].N          = N;
      thread_arg[j].DEG        = DEG;
      thread_arg[j].input_id   = (j < P/2) ? 0 : 1;
      thread_arg[j].barrier    = &barrier;
      thread_arg[j].d_count    = &d_count;
      
      //  Assign different data structures based on input_id
      if (thread_arg[j].input_id == 0) {
         // First half of threads get first input data
         thread_arg[j].Q          = Q;
         thread_arg[j].D          = D;
         thread_arg[j].W          = W;
         thread_arg[j].W_index    = W_index;
         thread_arg[j].exist      = exist;
      } else {
         // Second half of threads get second input data
         thread_arg[j].Q          = Q2;
         thread_arg[j].D          = D2;
         thread_arg[j].W          = W2;
         thread_arg[j].W_index    = W_index2;
         thread_arg[j].exist      = exist2;
      }
   }

   //for clock time
   struct timespec requestStart, requestEnd;
   clock_gettime(CLOCK_REALTIME, &requestStart);

   // Enable Graphite performance and energy models
   //CarbonEnableModels();

   //create threads
   for(int j = 1; j < P; j++) {
      pthread_create(thread_handle+j,
            NULL,
            do_work,
            (void*)&thread_arg[j]);
   }
   do_work((void*) &thread_arg[0]);

   //join threads
   for(int j = 1; j < P; j++) { //mul = mul*2;
      pthread_join(thread_handle[j],NULL);
   }

   // Disable Graphite performance and energy models
   //CarbonDisableModels();

   //read clock for time
   clock_gettime(CLOCK_REALTIME, &requestEnd);
   double accum = ( requestEnd.tv_sec - requestStart.tv_sec ) + ( requestEnd.tv_nsec - requestStart.tv_nsec ) / BILLION;
   printf( "Elapsed time: %lfs\n", accum );

   //printf("\ndistance:%d \n",D[N-1]);

   make_dot_graph(W,W_index,exist,D,N,DEG,"rgraph.dot");

   //for distance values check
   FILE * pfile;
   pfile = fopen("myfile.txt","w");
   fprintf (pfile,"distances:\n");
   for(int i = 0; i < N; i++) {
      if(D[i] != INT_MAX) {
         fprintf(pfile,"distance(%d) = %d\n", i, D[i]);
      }
   }
   fclose(pfile);
   printf("\n");

   return 0;
}

int initialize_single_source(int*  D,
      int*  Q,
      int   source,
      int   N)
{
   for(int i = 0; i < N; i++)
   {
      D[i] = INT_MAX;
      Q[i] = 1;
   }

   D[source] = 0;
   return 0;
}

int get_local_min(volatile int* Q, volatile int* D, int start, int stop, int N, int** W_index, int** W, int u)
{
   int min = INT_MAX;
   int min_index = N;

   for(int i = start; i < stop; i++) 
   {
      if(W_index[u][i]==-1 || W[u][i]==INT_MAX)
         continue;
      if(D[i] < min && Q[i]) 
      {
         min = D[i];
         min_index = W_index[u][i];
      }
   }
   return min_index;
}

void relax(int u, int i, volatile int* D, int** W, int** W_index, int N)
{
   if((D[W_index[u][i]] > (D[u] + W[u][i]) && (W_index[u][i]!=-1 && W_index[u][i]<N && W[u][i]!=INT_MAX)))
      D[W_index[u][i]] = D[u] + W[u][i];
}

void init_weights(int N, int DEG, int** W, int** W_index)
{
   // Initialize to -1
   for(int i = 0; i < N; i++)
      for(int j = 0; j < DEG; j++)
         W_index[i][j]= -1;

   // Populate Index Array
   for(int i = 0; i < N; i++)
   {
      int last = 0;
      for(int j = 0; j < DEG; j++)
      {
         if(W_index[i][j] == -1)
         {
            int neighbor = i + j;//rand()%(max);
            if(neighbor > last)
            {
               W_index[i][j] = neighbor;
               last = W_index[i][j];
            }
            else
            {
               if(last < (N-1))
               {
                  W_index[i][j] = (last + 1);
                  last = W_index[i][j];
               }
            }
         }
         else
         {
            last = W_index[i][j];
         }
         if(W_index[i][j]>=N)
         {
            W_index[i][j] = N-1;
         }
      }
   }

   // Populate Cost Array
   for(int i = 0; i < N; i++)
   {
      for(int j = 0; j < DEG; j++)
      {
         double v = drand48();
         /*if(v > 0.8 || W_index[i][j] == -1)
           {       W[i][j] = MAX;
           W_index[i][j] = -1;
           }

           else*/ if(W_index[i][j] == i)
         W[i][j] = 0;

         else
            W[i][j] = (int) (v*100) + 1;
         //printf("   %d  ",W_index[i][j]);
      }
      //printf("\n");
   }
}
