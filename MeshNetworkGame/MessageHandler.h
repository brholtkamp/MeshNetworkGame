#ifndef __MESSAGE_HANDLER_H__
#define __MESSAGE_HANDLER_H__
#include <string.h>
#include <memory>

#include "Message.h"
#include "MeshNode.h"

class MeshNode;

class MessageHandler {
public:
    virtual void handleMessage(Message message) = 0;
    void setMeshNode(std::weak_ptr<MeshNode> _node) { node = _node; }
    std::vector<std::string> getMessageTypes() { return messageTypes; }
protected:
    std::weak_ptr<MeshNode> node;
    std::vector<std::string> messageTypes;
};

#endif //__MESSAGE_HANDLER_H__