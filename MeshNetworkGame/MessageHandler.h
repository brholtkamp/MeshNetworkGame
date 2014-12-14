#ifndef __MESSAGE_HANDLER_H__
#define __MESSAGE_HANDLER_H__
#include <SFML/Network.hpp>
#include <json/json.h>
#include <vector>
#include <memory>

#include "MeshNode.h"

class MeshNode;

class MessageHandler {
public:
    virtual void handleMessage(std::string sender, std::string type, Json::Value message) = 0;
    void setMeshNode(std::unique_ptr<MeshNode> _node) { node = std::move(_node); }
    std::vector<std::string> getMessageTypes() { return messageTypes; }
protected:
    std::unique_ptr<MeshNode> node;
    std::vector<std::string> messageTypes;
};

#endif //__MESSAGE_HANDLER_H__