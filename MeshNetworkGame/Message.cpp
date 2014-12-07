#include "Message.h"

Message::Message() {

}

Message::Message(sf::IpAddress _address, unsigned short _port, std::string _type, Json::Value _message) {
    destination.address = _address;
    destination.port = _port;
    type = _type;
    message = _message;
}

Message::~Message() {
    pathway.clear();
}

void Message::setAddress(sf::IpAddress _address) {
    destination.address = _address;
}

sf::IpAddress Message::getAddress() {
    return destination.address;
}

void Message::setPort(unsigned short _port) {
    destination.port = _port;
}

unsigned short Message::getPort() {
    return destination.port;
}

void Message::setType(std::string _type) {
    type = _type;
}

std::string Message::getType() {
    return type;
}

void Message::setMessage(Json::Value _message) {
    message = _message;
}

Json::Value Message::getMessage() {
    return message;
}

bool Message::addToPathway(sf::IpAddress address, unsigned short port) {
    Path newPath;
    newPath.address = address;
    newPath.port = port;

    for (auto path : pathway) {
        if (path.address == newPath.address && path.port == newPath.port) {
            Log(address.toString() + ":" + std::to_string(port) + " already exists on pathway");
            return false;
        }
    }

    pathway.push_back(newPath);
    return true;
}

bool Message::removeFromPathway(sf::IpAddress address, unsigned short port) {
    for (auto path = pathway.begin(); path != pathway.end(); ++path) {
        if (path->address == address && path->port == port) {
            pathway.erase(path);
            return true;
        }
    }

    Log (address.toString() + ":" + std::to_string(port) + " does not exist on pathway");
    return false;
}

std::vector<Path> Message::getPathway() {
    return pathway;
}

Path Message::getFrontOfPathway() {
    return pathway.front();
}

std::string Message::toString() {
    std::stringstream output;
    output << "Message to " << destination.address << ":" << destination.port << " via:" << std::endl;

    for (auto path : pathway) {
        output << "\t" << path.address << ":" << path.port << std::endl;
    }

    output << "Contents of the message are: " << std::endl << message.toStyledString();

    return output.str();
}

Json::Value toJSON(Message message) {
    Json::Value result;

    Json::Value destination;
    destination["address"] = message.getAddress().toString();
    destination["port"] = message.getPort();
    result["destination"] = destination;

    for (auto path : message.getPathway()) {
        Json::Value pathValue;
        pathValue["address"] = path.address.toString();
        pathValue["port"] = path.port;
        result["pathway"].append(pathValue);
    }

    result["type"] = message.getType();

    result["message"] = message.getMessage();

    return result;
}

Message fromJSON(Json::Value message) {
    Message result;

    result.setAddress(sf::IpAddress(message["destination"]["address"].asString()));
    result.setPort(static_cast<unsigned short>(message["destination"]["port"].asUInt()));
    result.setType(message["type"].asString());
    result.setMessage(message["message"]);

    for (unsigned int i = 0; i < message["pathway"].size(); i++) {
        result.addToPathway(sf::IpAddress(message["pathway"][i].asString()), static_cast<unsigned short>(message["pathway"][i].asUInt()));
    }

    return result;
}