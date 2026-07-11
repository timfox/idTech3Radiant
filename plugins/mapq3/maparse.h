#pragma once

class EntityCreator;
namespace scene { class Node; }
class TextInputStream;

void MayaAscii_Read( scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable );
