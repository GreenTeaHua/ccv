/**********************************************************
 * C-based/Cached/Core Computer Vision Library
 * Liu Liu, 2010-02-01
 **********************************************************/

/**********************************************************
 * CCV - Neural Network Collection
 **********************************************************/

#ifndef GUARD_ccv_nnc_h
#define GUARD_ccv_nnc_h

#include <ccv.h>

enum {
	// These are the list of computation kernels.
	CCV_NNC_COMPUTE_CUSTOM = 0,
	CCV_NNC_COMPUTE_NOOP,
	// For neural networks
	CCV_NNC_COMPUTE_CONVOLUTION_FORWARD,
	CCV_NNC_COMPUTE_CONVOLUTION_BACKWARD,
	CCV_NNC_COMPUTE_MAX_POOL_FORWARD,
	CCV_NNC_COMPUTE_MAX_POOL_BACKWARD,
	CCV_NNC_COMPUTE_AVERAGE_POOL_FORWARD,
	CCV_NNC_COMPUTE_AVERAGE_POOL_BACKWARD,
	CCV_NNC_COMPUTE_SOFTMAX_FORWARD,
	CCV_NNC_COMPUTE_SOFTMAX_BACKWARD,
	CCV_NNC_COMPUTE_BATCH_NORM_FORWARD,
	CCV_NNC_COMPUTE_BATCH_NORM_BACKWARD,
	CCV_NNC_COMPUTE_RELU_FORWARD,
	CCV_NNC_COMPUTE_RELU_BACKWARD,
	// BLAS
	CCV_NNC_COMPUTE_AXPY_FORWARD,
	CCV_NNC_COMPUTE_AXPY_BACKWARD,
	CCV_NNC_COMPUTE_GEMM_FORWARD,
	CCV_NNC_COMPUTE_GEMM_BACKWARD,
	// Element-wise computation
	CCV_NNC_COMPUTE_EWSUM_FORWARD,
	CCV_NNC_COMPUTE_EWSUM_BACKWARD,
	CCV_NNC_COMPUTE_EWPROD_FORWARD,
	CCV_NNC_COMPUTE_EWPROD_BACKWARD,
	CCV_NNC_COMPUTE_EWDIV_FORWARD,
	CCV_NNC_COMPUTE_EWDIV_BACKWARD,
	CCV_NNC_COMPUTE_EWEXP_FORWARD,
	CCV_NNC_COMPUTE_EWEXP_BACKWARD,
	CCV_NNC_COMPUTE_EWLOG_FORWARD,
	CCV_NNC_COMPUTE_EWLOG_BACKWARD,
	// Other transforms
	CCV_NNC_COMPUTE_SET_FORWARD,
	CCV_NNC_COMPUTE_SET_BACKWARD,
	CCV_NNC_COMPUTE_DATA_TRANSFER_FORWARD,
	CCV_NNC_COMPUTE_DATA_TRANSFER_BACKWARD,
	CCV_NNC_COMPUTE_FORMAT_TRANSFORM_FORWARD,
	CCV_NNC_COMPUTE_FORMAT_TRANSFORM_BACKWARD,
	CCV_NNC_COMPUTE_COUNT,
};

enum {
	// Attributes that enable tensor allocation optimization
	CCV_NNC_COMPUTE_ATTR_INPLACE      = 0x01, // Is it a inplace operation? (Thus, the input tensor can be the same as the output tensor). This is actually a stronger assumption than it seems. It says that the input tensors can be the same as any of the output tensors. Thus, input tensors of [a, b] and output tensors of [b, a] or [a, a] or [b, b] are perfectly supported if your compute node supports this flag.
	// Attributes that enable symbolic graph simplification
	CCV_NNC_COMPUTE_ATTR_PASSTHROUGH  = 0x02, // This doesn't compute anything, but pass the first n tensors to the output (useful for backprop that is identical).
	CCV_NNC_COMPUTE_ATTR_OUTPUT_ONES  = 0x04, // All the output tensors are 1s (unit).
	CCV_NNC_COMPUTE_ATTR_NULL_IS_ONES = 0x08, // Accept nullptr input as if these are tensors with 1s (unit).
};

typedef struct {
	const char* name;
	int attrs; // List of attributes for this computation. These attributes enables some of the symbolic graph / tensor allocation optimizations, it needs to be implemented exactly the same cross different backends.
	struct {
		uint64_t input;
		uint64_t output;
	} bit_patterns[4]; // Maximum allow 4 patterns for input / output pairs, if this is not enough, we can always add more.
} ccv_nnc_compute_attr_t;

enum {
	CCV_NNC_ACCUMULATE_OUTPUT = 0x01, // Enable accumulate outputs.
	CCV_NNC_ZERO_MEMORY_ALLOC = 0x02, // Don't allocate any extra memory for this operation.
};

enum {
	CCV_NNC_EXEC_SUCCESS = 0,
	CCV_NNC_EXEC_INVALID = -1, // Invalid input.
	CCV_NNC_EXEC_NO_KERNEL = -2,
	CCV_NNC_EXEC_OOM = -3,
};

typedef struct {
	struct {
		int dim[CCV_NNC_MAX_DIM_ALLOC];
	} size; /**< [size] The window size for the layer. For full connect layer, it is 1 because it is 1x1 convolutional layer with count of filters */
	union {
		struct {
			int count; /**< [convolution.count] The number of filters for convolutional layer. */
		} convolution;
		struct {
		} pool;
		struct {
			float kappa; /**< [rnorm.kappa] As of b[i] = a[i] / (rnorm.kappa + rnorm.alpha * sum(a, i - rnorm.size / 2, i + rnorm.size / 2)) ^ rnorm.beta */
			float alpha; /**< [rnorm.alpha] See **rnorm.kappa**. */
			float beta; /**< [rnorm.beta] See **rnorm.kappa**. */
		} rnorm;
		struct {
			float a[3]; /**< BLAS scalars. */
			int count; /**< [blas.count] The number of outputs for blas layer. */
		} blas;
		void* userdata;
	};
} ccv_nnc_cmd_param_t;

typedef struct {
	struct {
		int dim[CCV_NNC_MAX_DIM_ALLOC];
	} stride;
	struct {
		int begin[CCV_NNC_MAX_DIM_ALLOC];
		int end[CCV_NNC_MAX_DIM_ALLOC];
	} border;
} ccv_nnc_hint_t;

typedef struct ccv_nnc_stream_context_s ccv_nnc_stream_context_t;

typedef struct ccv_nnc_cmd_s {
	int compute;
	int backend;
	int algorithm;
	ccv_nnc_cmd_param_t info;
	// This has to be the same as the ccv_nnc_cmd_exec_f type.
	// This is for type CCV_NNC_COMPUTE_CUSTOM
	int(*exec)(const struct ccv_nnc_cmd_s cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size, const ccv_nnc_stream_context_t* stream_context);
} ccv_nnc_cmd_t;

// For forward functions, the input tensors and output tensors can be arbitrary.
// However, for backward functions (backpropagation, or gradient functions in other libs),
// the input is: 0~m-1: gradient for output tensors, 1~n: input tensors for forward functions, n+1~n+m: output tensors for forward functions,
// the output is: 0~n-1: output gradients w.r.t. input tensors.
// Which input / output tensors can be ignored can be specified in the compute config structs.
typedef int(*ccv_nnc_cmd_exec_f)(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size, const ccv_nnc_stream_context_t* stream_context);

typedef int(*ccv_nnc_cmd_autotune_f)(const ccv_nnc_cmd_t cmd, const size_t max_workspace_size, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size, const ccv_nnc_stream_context_t* stream_context);

typedef struct {
	int tensor_formats; /**< [formats] The supported formats for this API implementation. */
	int tensor_memory; /**< [memory] The supported tensor memory type for this API implementation. */
	int algorithms; /**< [algorithms] Number of algorithms variation. */
	ccv_nnc_cmd_exec_f exec;
	ccv_nnc_cmd_autotune_f autotune;
} ccv_nnc_cmd_api_t;

/**
 * Level-0 API
 */

void ccv_nnc_init(void);

/**
 * Level-1 API
 */

// For tensor
CCV_WARN_UNUSED(ccv_nnc_tensor_t*) ccv_nnc_tensor_new(const void* ptr, const ccv_nnc_tensor_param_t params, const int flags);
// Allocating on stack
CCV_WARN_UNUSED(ccv_nnc_tensor_t) ccv_nnc_tensor(const void* ptr, const ccv_nnc_tensor_param_t params, const int flags);
void ccv_nnc_tensor_free(ccv_nnc_tensor_t* tensor);
CCV_WARN_UNUSED(ccv_nnc_tensor_view_t*) ccv_nnc_tensor_view_new(const ccv_nnc_tensor_t* tensor, const int ofs[CCV_NNC_MAX_DIM_ALLOC], const int dim[CCV_NNC_MAX_DIM_ALLOC]);
// Allocating on stack
CCV_WARN_UNUSED(ccv_nnc_tensor_view_t) ccv_nnc_tensor_view(const ccv_nnc_tensor_t* tensor, const int ofs[CCV_NNC_MAX_DIM_ALLOC], const int dim[CCV_NNC_MAX_DIM_ALLOC]);
void ccv_nnc_tensor_view_free(ccv_nnc_tensor_view_t* tensor_view);
// All these functions afterwards should be compatible with both tensor and tensor view unless assertion.
void ccv_nnc_tensor_zero(void* tensor);
int ccv_nnc_tensor_eq(const ccv_nnc_tensor_t* a, const ccv_nnc_tensor_t* b);

// For computation node
// Return high precision time unit.
uint64_t ccv_nnc_cmd_mono_time(void);
int ccv_nnc_cmd_backend(const char* name);
CCV_WARN_UNUSED(const char*) ccv_nnc_cmd_compute_name(const int compute);
CCV_WARN_UNUSED(const char*) ccv_nnc_cmd_backend_name(const int backend);
CCV_WARN_UNUSED(ccv_nnc_cmd_t) ccv_nnc_cmd(const int compute, ccv_nnc_cmd_exec_f exec, const ccv_nnc_cmd_param_t params, const int flags);
// Verify the hint
CCV_WARN_UNUSED(int) ccv_nnc_hint_verify(const ccv_nnc_hint_t hint, const ccv_nnc_cmd_param_t cmd, const ccv_nnc_tensor_param_t a, const ccv_nnc_tensor_param_t b);
// Auto find the best hint for a given input / output (on forward pass only).
CCV_WARN_UNUSED(ccv_nnc_hint_t) ccv_nnc_hint_auto(const ccv_nnc_cmd_param_t cmd, const ccv_nnc_tensor_param_t a, const ccv_nnc_tensor_param_t b);
// Auto find the outputs for the given inputs / hint.
void ccv_nnc_hint_tensor_auto(const ccv_nnc_cmd_t cmd, const ccv_nnc_tensor_param_t* inputs, const int input_size, const ccv_nnc_hint_t hint, ccv_nnc_tensor_param_t* outputs, const int output_size);
// Run autotune to find the best kernel and configuration for the given input, returned is the modified
// cmd that contains the updated configuration.
CCV_WARN_UNUSED(ccv_nnc_cmd_t) ccv_nnc_cmd_autotune(const ccv_nnc_cmd_t cmd, const size_t max_workspace_size, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size, const ccv_nnc_stream_context_t* stream_context);
int ccv_nnc_cmd_exec(const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, const int flags, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size, const ccv_nnc_stream_context_t* stream_context);
int ccv_nnc_cmd_attr(const ccv_nnc_cmd_t cmd, const int flags);
int ccv_nnc_cmd_is_forward(const ccv_nnc_cmd_t cmd);
int ccv_nnc_cmd_is_backward(const ccv_nnc_cmd_t cmd);

// Control flow constructs
// Follow heavily based along CUDA's stream / event idea.
enum {
	CCV_STREAM_CONTEXT_CPU = 0x1,
	CCV_STREAM_CONTEXT_GPU = 0x2,
};
#define CCV_STREAM_GET_CONTEXT(type) ((type) & 0x3)
#define CCV_STREAM_GET_DEVICE(type) ((type) & 0xff00)
#define CCV_STREAM_GET_DEVICE_ID(type) (CCV_STREAM_GET_DEVICE(type) >> 8)
// Flag is a combination of CPU / GPU and DEVICE_ID
CCV_WARN_UNUSED(ccv_nnc_stream_context_t*) ccv_nnc_stream_context_new(int type);
void ccv_nnc_stream_context_wait(const ccv_nnc_stream_context_t* stream);
void ccv_nnc_stream_context_free(ccv_nnc_stream_context_t* stream_context);

typedef struct ccv_nnc_stream_signal_s ccv_nnc_stream_signal_t;

CCV_WARN_UNUSED(ccv_nnc_stream_signal_t*) ccv_nnc_stream_signal_new(int type);
void ccv_nnc_stream_context_emit_signal(const ccv_nnc_stream_context_t* stream, const ccv_nnc_stream_signal_t* signal);
void ccv_nnc_stream_context_wait_signal(const ccv_nnc_stream_context_t* stream, const ccv_nnc_stream_signal_t* signal);
void ccv_nnc_stream_signal_free(ccv_nnc_stream_signal_t* signal);

/**
 * Level-2 API
 */

typedef struct ccv_nnc_graph_s ccv_nnc_graph_t;

typedef struct {
	int32_t d;
	const ccv_nnc_graph_t* graph;
} ccv_nnc_graph_exec_t;

#define CCV_NO_GRAPH_EXEC(exec) ((exec).graph == 0)

// Create an empty graph.
// Note that all graph mutation methods are not thread-safe.
// You should only operate the graph in serial fashion.
CCV_WARN_UNUSED(ccv_nnc_graph_t*) ccv_nnc_graph_new(void);
// Create a node with specific command execution, as well as its inputs & outputs.
// Underlying, the graph maintains the backing object for the node, and all you get is
// a on-stack object to index the backing object from the graph.
CCV_WARN_UNUSED(ccv_nnc_graph_exec_t) ccv_nnc_graph_exec(const ccv_nnc_graph_t* graph, const ccv_nnc_cmd_t cmd, const ccv_nnc_hint_t hint, ccv_nnc_tensor_t* const* inputs, const int input_size, ccv_nnc_tensor_t** outputs, const int output_size);
// Concatenate input graph nodes with an output graph node to create a new graph.
// Return non-zero if cannot concat successfully.
int ccv_nnc_graph_exec_concat(const ccv_nnc_graph_t* graph, const ccv_nnc_graph_exec_t source, const ccv_nnc_graph_exec_t destination);
// Run the autotune function for all the inputs / outputs, afterwards, assigning the optimized cmd back.
void ccv_nnc_graph_autotune(const ccv_nnc_graph_t* graph, const size_t max_workspace_size, const int flags, const ccv_nnc_graph_exec_t* sources, const int source_size, const ccv_nnc_graph_exec_t* destinations, const int destination_size);
// Run the graph from source nodes all the way to the destination nodes.
void ccv_nnc_graph_run(const ccv_nnc_graph_t* graph, const int flags, const ccv_nnc_graph_exec_t* sources, const int source_size, const ccv_nnc_graph_exec_t* destinations, const int destination_size);
// This graph, and its relevant auxiliary objects (opaque to user) are deallocated.
void ccv_nnc_graph_free(ccv_nnc_graph_t* graph);

/**
 * Level-3 API
 */

typedef struct ccv_nnc_symbolic_graph_s ccv_nnc_symbolic_graph_t;

// Opaque pointer to an arena of allocated tensors.
typedef struct ccv_nnc_tensor_arena_s ccv_nnc_tensor_arena_t;

// Opaque pointer to an arena of allocated execs.
typedef struct ccv_nnc_graph_exec_arena_s ccv_nnc_graph_exec_arena_t;

typedef struct {
	ccv_nnc_tensor_param_t info;
	int32_t d;
	const ccv_nnc_symbolic_graph_t* graph;
} ccv_nnc_tensor_symbol_t;

typedef struct {
	int32_t d;
	const ccv_nnc_symbolic_graph_t* graph;
} ccv_nnc_graph_exec_symbol_t;

// Create an empty symbolic graph.
// Note that all graph mutation methods are not thread-safe.
// You should only operate the graph in serial fashion.

// Create a new symbolic graph. It is an opaque data structure that maintains the whole graph of computation in its symbolic form.
CCV_WARN_UNUSED(ccv_nnc_symbolic_graph_t*) ccv_nnc_symbolic_graph_new(void);
// Create an tensor symbol (thus, with no actual memory space allocation) in a symbolic graph.
CCV_WARN_UNUSED(ccv_nnc_tensor_symbol_t) ccv_nnc_tensor_symbol(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_tensor_param_t info, const char* name);
// Create an alias to the tensor symbol as tensor view (thus, pointing to the same memory region, but with a different header info and offset).
CCV_WARN_UNUSED(ccv_nnc_tensor_symbol_t) ccv_nnc_tensor_symbol_alias(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_tensor_symbol_t tensor_symbol, const int ofs[CCV_NNC_MAX_DIM_ALLOC], const int inc[CCV_NNC_MAX_DIM_ALLOC], const ccv_nnc_tensor_param_t info, const char* name);
// Create a graph node (an operation that takes a set of inputs and generates a set of outputs).
CCV_WARN_UNUSED(ccv_nnc_graph_exec_symbol_t) ccv_nnc_graph_exec_symbol(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_cmd_t cmd, ccv_nnc_tensor_symbol_t* const inputs, const int input_size, ccv_nnc_tensor_symbol_t* outputs, const int output_size, const char* name);
// The operation defaults to use `ccv_nnc_hint_auto` find the best hints for a set of inputs / outputs.
// However, you can also set your own hints. Return non-zero if cannot set successfully.
int ccv_nnc_graph_exec_symbol_set_hint(const ccv_nnc_symbolic_graph_t* graph, ccv_nnc_graph_exec_symbol_t exec, ccv_nnc_hint_t hint);
// Set the tensor symbol info again. Thus, its dimensionality depends on the tensor input.
int ccv_nnc_tensor_symbol_set(const ccv_nnc_symbolic_graph_t* graph, ccv_nnc_tensor_symbol_t tensor, const ccv_nnc_tensor_param_t info);
// Manually concatenate input graph nodes with an output graph node to create a new graph.
// Return non-zero if cannot concat successfully.
int ccv_nnc_graph_exec_symbol_concat(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_graph_exec_symbol_t source, const ccv_nnc_graph_exec_symbol_t destination);
// Automatic concatenate these nodes together based on its inputs / outputs.
// Return non-zero if cannot figure out.
// Imagining this is to generate the execution flow based on input tensors and output tensors.
int ccv_nnc_graph_exec_symbol_flow(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_graph_exec_symbol_t* execs, const int exec_size);
// Generate output that can be parsed by GraphViz (DOT language).
void ccv_nnc_symbolic_graph_dot(const ccv_nnc_symbolic_graph_t* graph, FILE* out);

typedef struct {
	ccv_nnc_tensor_t* tensor;
	ccv_nnc_tensor_symbol_t symbol;
} ccv_nnc_tensor_bind_t;

// Compile a symbolic graph into a graph that can be executed, and a set of tensors (opaque data structure tensor arena) are allocated based on which tensor symbols are the input and which are the outputs. The tensor allocation is done to minimize the required storage.
// tensor_binds provide custom binding for these tensors
// tensor_symbol_consts provide a list of "protected" tensors that is const to us.
void ccv_nnc_symbolic_graph_compile(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_tensor_bind_t* tensor_binds, const int tensor_binds_size, const ccv_nnc_graph_exec_symbol_t* sources, const int source_size, const ccv_nnc_graph_exec_symbol_t* destinations, const int destination_size, ccv_nnc_graph_t** graph_ref, ccv_nnc_tensor_arena_t** tensor_arena_ref, ccv_nnc_graph_exec_arena_t** graph_exec_arena_ref);
// Free the symbolic graph and its associated memory. Note that if you compiled a graph / tensor arena out of this symbolic graph, these won't be free'd.
void ccv_nnc_symbolic_graph_free(ccv_nnc_symbolic_graph_t* graph);
// Find corresponding tensor by a symbol from the tensor arena.
CCV_WARN_UNUSED(ccv_nnc_tensor_t*) ccv_nnc_tensor_from_symbol(const ccv_nnc_tensor_arena_t* tensor_arena, const ccv_nnc_tensor_symbol_t symbol);
// Free the opaque tensor arena structure.
void ccv_nnc_tensor_arena_free(ccv_nnc_tensor_arena_t* tensor_arena);
// Find corresponding graph exec by a exec symbol from graph exec arena.
CCV_WARN_UNUSED(ccv_nnc_graph_exec_t) ccv_nnc_graph_exec_from_symbol(const ccv_nnc_graph_exec_arena_t* graph_exec_arena, const ccv_nnc_graph_exec_symbol_t symbol);
// Free the opaque graph exec arena structure.
void ccv_nnc_graph_exec_arena_free(ccv_nnc_graph_exec_arena_t* graph_exec_arena);

/**
 * Level-4 API
 */
// Compute the backward graph, assuming the provided symbolic graph only contain the "forward" part from sources to destinations.
// This effectively is called the "autograd" or automatic differentiation process (specifically, "reverse AD") in other libs.
void ccv_nnc_symbolic_graph_backward(ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_graph_exec_symbol_t* sources, const int source_size, const ccv_nnc_graph_exec_symbol_t* destinations, const int destination_size, const ccv_nnc_tensor_symbol_t* f_symbols, const int f_symbol_size, const ccv_nnc_tensor_symbol_t* wrt_symbols, const int wrt_symbol_size);
// Get the symbol that contains the gradient. The list will be flushed if the ccv_nnc_symbolic_graph_backward function is called again.
CCV_WARN_UNUSED(ccv_nnc_tensor_symbol_t) ccv_nnc_tensor_symbol_for_backward(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_tensor_symbol_t symbol);
// This has to get the exec symbol from the tensor.
CCV_WARN_UNUSED(ccv_nnc_graph_exec_symbol_t) ccv_nnc_graph_exec_symbol_for_backward(const ccv_nnc_symbolic_graph_t* graph, const ccv_nnc_tensor_symbol_t symbol);
// TODO: A function to run while loop for RNN (need to figure out how to do book-keeping for bidirectional dynamic RNN).
// In that case, the computation graph still has no loops or cycles, but you can run it multiple times against different
// versions of the tensors (thus, the tensor is versioned, so you can "backpropagate through time".
int ccv_nnc_graph_while(const ccv_nnc_graph_t* graph, const ccv_nnc_tensor_arena_t* tensor_arena, const int version, const int flags, const ccv_nnc_graph_exec_t* sources, const int source_size, const ccv_nnc_graph_exec_t* destinations, const int destination_size);

#endif
