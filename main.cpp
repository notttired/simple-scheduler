#define _XOPEN_SOURCE 600
#include <ucontext.h>
#include <assert.h>
#include <stdlib.h>
#include <iostream>

int STACK_SIZE = 64000;
int ID = 0;

int get_ID() {
    return ID++;
}

typedef enum task_status {
    READY,
    RUNNING,
    WAITING,
    TERMINATED,
} task_status;

typedef struct task {
    int tid;
    enum task_status status;
    ucontext_t* context;

    void (*op)(void*);
    void* arg;
} task;

typedef struct node {
    task* current_task;
    struct node* next;
} node;

typedef struct list {
    node* head;
    node* tail;
    int length;
} list;

void append(list* queue, task* new_task) {
    assert(queue);
    node* new_node = (node*) malloc(sizeof(node));
    assert(new_node);

    new_node->current_task = new_task;
    new_node->next = nullptr;

    if (!queue->head) {
        queue->head = new_node;
        queue->tail = new_node;
    } else {
        queue->tail->next = new_node;
        queue->tail = new_node;
    }

    queue->length++;
}

task* popleft(list* queue) {
    if (!queue || !queue->head) {
        return nullptr;
    }

    node* popped_node = queue->head;
    task* popped_task = popped_node->current_task;
    queue->head = queue->head->next;
    if (!queue->head) {
        queue->tail = nullptr;
    }
    free(popped_node);

    queue->length--;
    return popped_task;
}

void destroy_task(task* task) {
    assert(task);
    task->status = TERMINATED;
    if (task->context) {
        free(task->context);
    }
    if (task->arg) {
        free(task->arg);
    }
    free(task);
}

void destroy_node(node* curr) {
    if (curr->current_task) {
        destroy_task(curr->current_task);
    }
    free(curr);
}

void destroy_list(list* queue) {
    assert(queue);

    node* curr = queue->head;
    while (curr) {
        node* next = curr->next;
        destroy_node(curr);
        curr = next;
    }

    free(queue);
}

ucontext_t* initialize_context(ucontext_t* uc_link) {
    auto* ctx = (ucontext_t*) malloc(sizeof(ucontext_t));
    getcontext(ctx);
    ctx->uc_stack.ss_sp = (char*) malloc(STACK_SIZE);
    ctx->uc_stack.ss_size = STACK_SIZE;
    ctx->uc_link = uc_link;
    return ctx;
}

typedef void (*Operation)(void*);

task* create_task(Operation op, void* arg, ucontext_t* uc_link) {
    assert(uc_link);
    ucontext_t* ctx = initialize_context(uc_link);
    makecontext(ctx, (void (*)(void))op, 1, arg);

    task* new_task = (task*) malloc(sizeof(task));
    assert(new_task);
    new_task->tid = get_ID();
    new_task->status = READY;
    new_task->context = ctx;
    new_task->op = op;
    new_task->arg = arg;

    return new_task;
}

void test(void* arg) {
    printf("Ran function\n");
}

int main() {
    printf("Starting\n");
    ucontext_t* main_ctx = initialize_context(nullptr);

    list* queue = (list*) malloc(sizeof(list));

    for (int i = 0; i < 100; ++i) {
        task* new_task = create_task(test, nullptr, main_ctx);
        append(queue, new_task);
    }

    while (true) {
        if (queue->head) {
            task* task = popleft(queue);
            assert(task);
            swapcontext(main_ctx, task->context);
            destroy_task(task);
        } else {
            printf("Finished\n");
            break;
        }
    }
}