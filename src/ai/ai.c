#include <time.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <stdbool.h>


#include "ai.h"
#include "utils.h"
#include "hashtable.h"
#include "chessformer.h"
#include "node.h"

/**
 * Creates an intial state node based on a chessformer graph
*/
node_t *create_init_node(chessformer_t* chessformer) {
	node_t *n = (node_t *) malloc(sizeof(node_t));
	assert(n);

	n->depth = 0;
	n->num_childs = 0;
	n->move_delta_x = 0;
	n->move_delta_y = 0;
	n->state.map = (char **) malloc(sizeof(char *) * chessformer->lines);
	assert(n->state.map);
	int mapLength = chessformer->num_chars_map / chessformer->lines;
	for(int i = 0; i < chessformer->lines; i++){
		n->state.map[i] = (char *) malloc(sizeof(char) * (mapLength + 1));
		assert(n->state.map[i]);
		for(int j = 0; j < mapLength; j++){
			n->state.map[i][j] = chessformer->map[i][j];
		}
		n->state.map[i][mapLength] = '\0';
	}

	n->state.player_x = chessformer->player_x;
	n->state.player_y = chessformer->player_y;

	n->parent = NULL;

	return n;
}

/**
 * Copy a src into a dst state
*/
void copy_state(chessformer_t* init_data, state_t* dst, state_t* src){
	dst->map = malloc(sizeof(char *) * init_data->lines);
	assert(dst->map);
	for (int i = 0; i < init_data->lines; i++){
		int width = strlen(src->map[i]) + 1;
		dst->map[i] = malloc(width);
		assert(dst->map[i]);
		memcpy(dst->map[i], src->map[i], width);
	}
	dst->player_x = src->player_x;
	dst->player_y = src->player_y;
}

node_t* create_node(chessformer_t* init_data, node_t* parent){
	node_t *new_n = (node_t *) malloc(sizeof(node_t));
	new_n->parent = parent;
	new_n->depth = parent->depth + 1;
	copy_state(init_data, &(new_n->state), &(parent->state));
	return new_n;
}

/**
 * Apply an action to node n, create a new node resulting from 
 * executing the action, and return if the player moved
*/
bool applyAction(chessformer_t *init_data, node_t *n, 
					node_t **new_node, int move_delta_x, int move_delta_y){
	bool player_moved = false;

    *new_node = create_node(init_data, n);
    (*new_node)->move_delta_x = move_delta_x;
	(*new_node)->move_delta_y = move_delta_y;

    player_moved = execute_move(init_data, &((*new_node)->state), 
								move_delta_x, move_delta_y);

	return player_moved;
}

/**
 * Frees the memory of the current state
*/
void free_state(chessformer_t *init_data, node_t* n){
	// Free the dynamically allocated map strings
    for (int i = 0; i < init_data->lines; i++) {
        free(n->state.map[i]);  // Free each row of the map
    }

    // Free the map string pointers
    free(n->state.map);

    // Free the node itself
    free(n);
}

/**
 * Given a 2D map, returns a 1D map
*/
void flatten_map(chessformer_t* init_data, char **dst_map, char **src_map){

	int current_i = 0;
	for (int i = 0; i < init_data->lines; ++i)
	{
		int width = strlen(src_map[i]);
		for (int j = 0; j < width; j++){
			(*dst_map)[current_i] = src_map[i][j];
			current_i++;
		}
	}
}

/**
 * Check if all capturable pieces are captured. 
 * Returns true if so, false if not  
*/
bool winning_state(chessformer_t chessformer){
	for (int i = 0; i < chessformer.lines; i++) {
		for (int j = 0; j < 
				chessformer.num_chars_map/chessformer.lines; j++) {
			if (chessformer.map[i][j] == '$') {
				return false;
			}
		}
	}
	
	return true;
}

/**
 * Saves the final solution found 
*/
char *saveSolution(node_t *finalNode) {
	int hierarchyCount = 0;
	node_t *current = finalNode;
	while(current){
		hierarchyCount++;
		current = current->parent;
	}
	char *soln = (char *) malloc(sizeof(char) * hierarchyCount * 2 + 1);
	assert(soln);
	
	current = finalNode;
	int left = hierarchyCount - 1;
	while(current){
		if(! current->parent){
			current = current->parent;
			continue;
		}
		left--;
		char x = current->parent->state.player_x + 
								current->move_delta_x + '`';
		char y = current->parent->state.player_y + 
								current->move_delta_y + '0';
		soln[2 * left] = x;
		soln[2 * left + 1] = y;
		current = current->parent;
	}

	soln[2 * (hierarchyCount - 1)] = '\0';

	return soln;
}

// Create a struct for a queue node
typedef struct queue_s {
    node_t* node;
    struct queue_s* next;
} queue_t;

/**
 * Place a node into the queue, update the queue
*/
queue_t* enqueue(queue_t* head, node_t* node) {
    queue_t* new_element = (queue_t*)malloc(sizeof(queue_t));
    new_element->node = node;
    new_element->next = NULL;

    if (head == NULL) {
        return new_element;
    }

    queue_t* temp = head;
    while (temp->next != NULL) {
        temp = temp->next;
    }
    temp->next = new_element;
    return head;
}

/**
 * Removes the node from the queue, update the queue
*/
queue_t* dequeue(queue_t* head, node_t** node) {
    if (head == NULL) {
        *node = NULL;
        return NULL;
    }
    *node = head->node;
    queue_t* temp = head->next;
    free(head);
    return temp;
}

/**
 * Returns true if the queue is empty, false if not
*/
bool is_queue_empty(queue_t* head) {
    return head == NULL;
}

#define DEBUG 0

#include <assert.h>
/**
 * Find a solution by exploring all possible paths
 */
void find_solution(chessformer_t* init_data, bool show_solution){
	// Statistics variables
	// Keep track of solving time
	clock_t start = clock();
	unsigned int exploredNodes = 0;
	unsigned int generatedNodes = 0;
	unsigned int duplicatedNodes = 0;
	// Maximum depth limit so far - e.g. in Iterative Deepening
	// int max_depth = 0;
	unsigned solution_size = 0;
	// Solution String containing the sequence of moves
	char* solution = NULL;

	// Initialize explored nodes array
    node_t** exploredNodesArray = malloc(INITIAL_CAPACITY * sizeof(node_t*));
    size_t capacity = INITIAL_CAPACITY;

	// Optimisation (Algorithm 2 setup)
	HashTable hashTable;
	// Key size
	size_t htKeySize = sizeof(char) * init_data->num_chars_map;
	// Data size - same as key size as we do not do anything with data pair.
	size_t htDataSize = sizeof(char) * init_data->num_chars_map;
	// Hash table capacity - limit not too important, but 26 * 9 covers all
	//		single piece locations on one maximum size board.
	size_t htCapacity = 26 * 9;
	ht_setup(&hashTable, htKeySize, htDataSize, htCapacity);

	// Buffer for dequeue
	chessformer_t current_state = *init_data; 
	current_state.soln = "";
	queue_t* queue = NULL;
	
	// Create the initial node
    node_t* initial_node = create_init_node(init_data);
    queue = enqueue(queue, initial_node);
    generatedNodes++;

	// Search Algorithm
    while (!is_queue_empty(queue)) {
        // Dequeue the next node to explore
        node_t* current_node;
        queue = dequeue(queue, &current_node);
        exploredNodes++;

		// Add current_node to explored nodes array
        if (exploredNodes > capacity) {
            // Increase capacity
            capacity *= 2;
            exploredNodesArray = realloc(exploredNodesArray, 
											capacity * sizeof(node_t*));
        }
		// Store the explored node
        exploredNodesArray[exploredNodes - 1] = current_node; 

        // Check if this node represents a winning state
        if (winning_condition(&current_state, &current_node->state)) {
            // Winning state found, save the solution
            solution = saveSolution(current_node);
            solution_size = current_node->depth;
            break;
        }

        // Explore possible moves from the current state
        for (int move_y = 1 - current_node->state.player_y; move_y <= 
				current_state.lines - current_node->state.player_y; move_y++) {
            for (int move_x = 1 - current_node->state.player_x; move_x <= 
					(current_state.num_chars_map / current_state.lines) - 
							current_node->state.player_x; move_x++) {
                // Skip if there is no movement
                if (move_x == 0 && move_y == 0) {
                    continue;
                }

				node_t* new_node;
				bool player_moved = applyAction(&current_state, 
									current_node, &new_node, move_x, move_y);

				// // UNOPTIMISED CODE
				// // Only add new states where the player actually moved
				// if (player_moved) {
				// 	// printf("The excecuted movements are %d, %d\n", move_x, move_y);
				// 	queue = enqueue(queue, new_node);
				// 	generatedNodes++;
				// }else{
				// 	// free new node
				// 	// printf("Player did not move for these %d %d\n",move_x, move_y);
   				// 	free_state(init_data, new_node);
				// }
				// // END UNOPTIMISED CODE

				// OPTIMISED CODE
				// Only add new states where the player actually moved
				if (player_moved) {
					char* flat_map = calloc(init_data->num_chars_map, 
												sizeof(char));
					assert(flat_map);
                    flatten_map(init_data, &flat_map, new_node->state.map);

					if (!ht_contains(&hashTable, flat_map)) {
						// Store the map in the hash table
                        ht_insert(&hashTable, flat_map, flat_map); 
                        queue = enqueue(queue, new_node);
                        generatedNodes++;
                    } else {
                        duplicatedNodes++;
						// Free the new node since it's a duplicate
                        free_state(init_data, new_node); 
					}

					free(flat_map);
				}else{
					// free new node
   					free_state(init_data, new_node);
				}
				// END OPTIMISED CODE
            }
        }
    }

	// Stop clock
	clock_t end = clock();
	double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

	// Clear memory
	// Free memory for nodes stored in explored table and generated nodes still 
	// 		in queue. In Algorithm 2 - also free flat_map used as temporary 
	// 		variable for storing map.

	// Free remaining nodes in the queue after solution is found
	while (!is_queue_empty(queue)) {
		node_t* leftover_node;
		queue = dequeue(queue, &leftover_node);
		free_state(init_data, leftover_node);
	}

	// Free all the explored nodes altogether
	for (int i = exploredNodes - 1; i >= 0; i--) {
		free_state(init_data, exploredNodesArray[i]);
	}
	// Free the explored nodes array itself
	free(exploredNodesArray);

	// In Algorithm 2, free hash table.
	ht_clear(&hashTable);
	ht_destroy(&hashTable);

	// Report statistics.

	// Show Solution	
	if(show_solution && solution != NULL) {
		play_solution(*init_data, solution);
	}

	// endwin();

	if(solution != NULL){
		printf("\nSOLUTION:                               \n");
		printf( "%s\n\n", solution);
		FILE *fptr = fopen("solution.txt", "w");
		if (fptr == NULL) {
			printf("Could not open file");
			return ;
		}
		fprintf(fptr,"%s\n", solution);
		fclose(fptr);
		
		free(solution);
	}

	printf("STATS: \n");
	printf("\tExpanded nodes: %'d\n\tGenerated nodes: %'d\n\tDuplicated nodes: %'d\n", exploredNodes, generatedNodes, duplicatedNodes);
	printf("\tSolution Length: %d\n", solution_size);
	printf("\tExpanded/seconds: %d\n", (int)(exploredNodes/cpu_time_used));
	printf("\tTime (seconds): %f\n", cpu_time_used);	
}



void solve(char const *path, bool show_solution)
{
	/**
	 * Load Map
	*/
	chessformer_t chessformer = make_map(path, chessformer);
	
	/**
	 * Count number of boxes and Storage locations
	*/
	map_check(chessformer);

	/**
	 * Locate player x,y position
	*/
	chessformer = find_player(chessformer);
	
	chessformer.base_path = path;

	find_solution(&chessformer, show_solution);

	for(int i = 0; i < chessformer.lines; i++){
		free(chessformer.map[i]);
		free(chessformer.map_save[i]);
	}
	free(chessformer.map);
	free(chessformer.map_save);
	free(chessformer.buffer);

}