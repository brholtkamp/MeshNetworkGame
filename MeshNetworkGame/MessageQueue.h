#ifndef __MESSAGE_QUEUE_H__ 
#define __MESSAGE_QUEUE_H__ 
#include <memory>
#include <mutex>
#include <deque>

template <class T>
class MessageQueue {
public:
    MessageQueue();
    ~MessageQueue();

    inline void push_back(T message);
    inline T get_next();
    inline unsigned int size();

private:
    std::mutex lock;
    std::deque<T> items;
};

template <class T>
MessageQueue<T>::MessageQueue() {

}

template <class T>
MessageQueue<T>::~MessageQueue() {

}

template <class T>
void MessageQueue<T>::push_back(T message) {
    lock.lock();
        items.push_back(message);
    lock.unlock();
}

template <class T>
T MessageQueue<T>::get_next() {
    lock.lock();
        T item = items.front();
        items.pop_front();
    lock.unlock();
    return item;
}

template <class T>
unsigned int MessageQueue<T>::size() {
    return items.size();
}

#endif //__MESSAGE_QUEUE_H__