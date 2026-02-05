// class design: resolve ABA problem using CASVD but need to work on hazard pointers later for SMR
#include <uAtomic.h> // For atomic operations if not using CASVD macros directly

class LockFreeQueue {
    // Structure to handle the ABA problem with a versioning counter
    union Link {
        struct {
            Node *ptr;        // Pointer to the node
            uintptr_t ticket; // Versioning counter to detect ABA
        };
        uintS_t atom;        // 64/128-bit atomic representation
    };

    struct Node {
        int data;
        Link next;
        Node( int d ) : data(d) { next.atom = 0; }
    };

    Link head, tail;

public:
    LockFreeQueue() {
        // Initialize with a dummy node to simplify head/tail logic
        Node* dummy = new Node(0);
        head.ptr = tail.ptr = dummy;
        head.ticket = tail.ticket = 0;
    }

    void enqueue( int value ) {
        Node* n = new Node( value );
        Link t, next;
        for ( ;; ) {
            t = tail;             // Copy current tail
            next = t.ptr->next;
            
            if ( t.atom == tail.atom ) { // Verify tail hasn't moved
                if ( next.ptr == nullptr ) {
                    // Attempt to link the new node to the end
                    if ( CASVD( t.ptr->next.atom, next.atom, (Link){ n, next.ticket + 1 }.atom ) ) {
                        break; // Successfully linked
                    }
                } else {
                    // Tail was lagging; try to move it forward
                    CASVD( tail.atom, t.atom, (Link){ next.ptr, t.ticket + 1 }.atom );
                }
            }
        }
        // Attempt to move tail to the newly inserted node
        CASVD( tail.atom, t.atom, (Link){ n, t.ticket + 1 }.atom );
    }

    bool dequeue( int &value ) {
        Link h, t, next;
        for ( ;; ) {
            h = head;
            t = tail;
            next = h.ptr->next;

            if ( h.atom == head.atom ) {
                if ( h.ptr == t.ptr ) {
                    if ( next.ptr == nullptr ) return false; // Queue is empty
                    // Tail is lagging; move it forward
                    CASVD( tail.atom, t.atom, (Link){ next.ptr, t.ticket + 1 }.atom );
                } else {
                    value = next.ptr->data;
                    // Attempt to move head forward
                    if ( CASVD( head.atom, h.atom, (Link){ next.ptr, h.ticket + 1 }.atom ) ) {
                        delete h.ptr; // Safe to delete the old dummy node
                        return true;
                    }
                }
            }
        }
    }
};