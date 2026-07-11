#pragma once

class EntityCreator;
namespace scene { class Node; }
class TextInputStream;

void UsdAscii_Read( scene::Node& root, TextInputStream& inputStream, EntityCreator& entityTable );
