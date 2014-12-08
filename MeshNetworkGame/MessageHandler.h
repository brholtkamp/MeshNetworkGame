#ifndef __MESSAGE_HANDLER_H__
#define __MESSAGE_HANDLER_H__
#include <string>
#include <memory>

#include "MeshNode.h"

class MeshNode;

class MessageHandler {
public:
    ~MessageHandler() { node.release(); }
    virtual void handleMessage(Json::Value message, sf::IpAddress fromAddress, unsigned short fromPort, std::string type) = 0;
    void setMeshNode(std::unique_ptr<MeshNode> _node) { node = std::move(_node); }
    std::vector<std::string> getMessageTypes() { return messageTypes; }
protected:
    std::unique_ptr<MeshNode> node;
    std::vector<std::string> messageTypes;
};

#endif //__MESSAGE_HANDLER_H__