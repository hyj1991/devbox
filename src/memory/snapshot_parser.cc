#include "snapshot_parser.h"

namespace snapshot_parser {
SnapshotParser::SnapshotParser(json profile) {
  nodes = profile["nodes"];
  edges = profile["edges"];
  strings = profile["strings"];
  snapshot = profile["snapshot"];
  if (snapshot["root_index"] != nullptr) {
    root_index = snapshot["root_index"];
  }
  json node_fields = snapshot["meta"]["node_fields"];
  json edge_fields = snapshot["meta"]["edge_fields"];
  node_field_length = node_fields.size();
  edge_field_length = edge_fields.size();
  node_count = nodes.size() / node_field_length;
  edge_count = edges.size() / edge_field_length;
  node_types = snapshot["meta"]["node_types"][0];
  edge_types = snapshot["meta"]["edge_types"][0];
  node_type_offset = IndexOf(node_fields, "type");
  node_name_offset = IndexOf(node_fields, "name");
  node_address_offset = IndexOf(node_fields, "id");
  node_self_size_offset = IndexOf(node_fields, "self_size");
  node_edge_count_offset = IndexOf(node_fields, "edge_count");
  node_trace_nodeid_offset = IndexOf(node_fields, "trace_node_id");
  edge_type_offset = IndexOf(edge_fields, "type");
  edge_name_or_index_offset = IndexOf(edge_fields, "name_or_index");
  edge_to_node_offset = IndexOf(edge_fields, "to_node");
  edge_from_node = new int[edge_count];
  first_edge_indexes = GetFirstEdgeIndexes();
  node_util = new snapshot_node::Node(this);
  edge_util = new snapshot_edge::Edge(this);
}

int SnapshotParser::IndexOf(json array, std::string target) {
  const char* t = target.c_str();
  int size = array.size();
  for (int i = 0; i < size; i++) {
    std::string str1 = array[i];
    if(strcmp(str1.c_str(), t) == 0) {
      return i;
    }
  }
  return -1;
}

int* SnapshotParser::GetFirstEdgeIndexes() {
  int* first_edge_indexes = new int[node_count];
  for(int node_ordinal = 0, edge_index = 0; node_ordinal < node_count; node_ordinal++) {
    first_edge_indexes[node_ordinal] = edge_index;
    int offset = static_cast<int>(nodes[node_ordinal * node_field_length + node_edge_count_offset]) * edge_field_length;
    for(int i = edge_index; i < offset; i += edge_field_length) {
      edge_from_node[i / edge_field_length] = node_ordinal;
    }
    edge_index += offset;
  }
  return first_edge_indexes;
}

void SnapshotParser::CreateAddressMap() {
  for(int ordinal = 0; ordinal < node_count; ordinal++) {
    int address = node_util->GetAddress(ordinal, false);
    address_map_.insert(AddressMap::value_type(address, ordinal));
  }
}

void SnapshotParser::ClearAddressMap() {
  address_map_.clear();
}

int SnapshotParser::SearchOrdinalByAddress(int address) {
  int count = address_map_.count(address);
  if(count == 0) {
    return -1;
  }
  int ordinal = address_map_.at(address);
  return ordinal;
}

void SnapshotParser::BuildTotalRetainer() {
  retaining_nodes_ = new int[edge_count] {0};
  retaining_edges_ = new int[edge_count] {0};
  first_retainer_index_ = new int[node_count + 1] {0};
  // every node's retainer count
  for(int to_node_field_index = edge_to_node_offset, l = edge_count; to_node_field_index < l; to_node_field_index += edge_field_length) {
    int to_node_index = edges[to_node_field_index];
    if(to_node_index % node_field_length != 0) {
      Nan::ThrowTypeError(Nan::New<v8::String>("node index id is wrong!").ToLocalChecked());
      return;
    }
    int ordinal_id = to_node_index / node_field_length;
    first_retainer_index_[ordinal_id] += 1;
  }
  // set first retainer index
  for(int i = 0, first_unused_retainer_slot = 0; i < node_count; i++) {
    int retainers_count = first_retainer_index_[i];
    first_retainer_index_[i] = first_unused_retainer_slot;
    retaining_nodes_[first_unused_retainer_slot] = retainers_count;
    first_unused_retainer_slot += retainers_count;
  }
  // for (index ~ index + 1)
  first_retainer_index_[node_count] = edge_count;
  // set retaining slot
  int next_node_first_edge_index = first_edge_indexes[0];
  for(int src_node_ordinal = 0; src_node_ordinal < node_count; src_node_ordinal++) {
    int first_edge_index = next_node_first_edge_index;
    next_node_first_edge_index = first_edge_indexes[src_node_ordinal + 1];
    for(int edge_index = first_edge_index; edge_index < next_node_first_edge_index; edge_index += edge_field_length) {
      int to_node_index = edges[edge_index + edge_to_node_offset];
      if(to_node_index % node_field_length != 0) {
        Nan::ThrowTypeError(Nan::New<v8::String>("to_node id is wrong!").ToLocalChecked());
        return;
      }
      int first_retainer_slot_index = first_retainer_index_[to_node_index / node_field_length];
      int next_unused_retainer_slot_index = first_retainer_slot_index + (--retaining_nodes_[first_retainer_slot_index]);
      // save retainer & edge
      retaining_nodes_[next_unused_retainer_slot_index] = src_node_ordinal;
      retaining_edges_[next_unused_retainer_slot_index] = edge_index;
    }
  }
}
}