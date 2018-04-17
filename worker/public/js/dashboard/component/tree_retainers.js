(function () {
  var TreeRetainers = {
    template: '#tree-template',
    data() {
      return { props: { label: 'name', isLeaf: 'exists' }, node: {}, type: 'retainers', loadMoreStatus: false }
    },
    props: ['rootid', 'nodeData', 'getNode', 'formatSize'],
    methods: {
      formatNode(data, retainer, raw) {
        raw = raw || {};
        raw.id = data.id;
        raw.key = `${Math.random().toString(36).substr(2)}`;
        raw.name = data.name;
        raw.address = data.address;
        raw.additional = `(type: ${data.type}, self_size: ${this.formatSize(data.self_size)})`;
        raw.retainers = data.retainers;
        raw.retainersEnd = data.retainers_end;
        raw.retainersCurrent = data.retainers_current;
        raw.retainersLeft = data.retainers_left;
        if (!retainer) {
          raw.expandPaths = [data.address];
        }
        if (retainer) {
          if (retainer.type === 'property' || retainer.type === 'element' || retainer.type === 'shortcut') {
            raw.edgeClass = 'property';
          }
          if (retainer.type === 'context') {
            raw.edgeClass = 'context';
          }
          raw.fromEdge = `${retainer.name_or_index}`
        }
        return raw;
      },
      formatPaths(parent, child, address) {
        child.expandPaths = parent.expandPaths.concat([address]);
        if (~parent.expandPaths.indexOf(address)) {
          child.exists = true;
          child.class = 'disabled';
        }
      },
      loadNode(node, resolve) {
        var vm = this;
        if (node.level === 0) {
          vm.node = node;
          vm.getNode(`/ordinal/${vm.rootid}?current=0&limit=${Devtoolx.limit}&type=retainers`)
            .then(data => resolve([vm.formatNode(data[0])]))
            .catch(err => vm.$message.error(err));
          return;
        }
        var data = node.data;
        if (node.level > 0) {
          if (data.retainers) {
            var ids = data.retainers.map(r => r.from_node).join(',');
            if (ids == '') return resolve([])
            vm.getNode(`/ordinal/${ids}/?current=0&limit=${Devtoolx.limit}&type=retainers`)
              .then((list) => {
                var result = list.map((r, i) => {
                  var result = vm.formatNode(r, data.retainers[i]);
                  vm.formatPaths(data, result, r.address);
                  return result;
                }).filter(r => r);
                if (!data.retainersEnd) {
                  result.push({
                    id: data.id,
                    loadMore: true,
                    retainersCurrent: data.retainersCurrent,
                    exists: true,
                    retainersLeft: data.retainersLeft
                  });
                }
                resolve(result);
              }).catch(err => vm.$message.error(err));
          }
        }
      },
      loadMore(node, rawdata) {
        var vm = this;
        var p = null;
        vm.loadMoreStatus = true;
        vm.getNode(`/ordinal/${rawdata.id}?current=${rawdata.retainersCurrent}&limit=${Devtoolx.limit}&type=retainers`)
          .then(parent => {
            p = parent = parent[0];
            var ids = parent.retainers.map(r => r.from_node).join(',');
            if (ids == '') return [];
            return vm.getNode(`/ordinal/${ids}/?current=0&limit=${Devtoolx.limit}&type=retainers`);
          }).then(list => {
            list.forEach((r, i) => {
              var data = vm.formatNode(r, p.retainers[i]);
              vm.formatPaths(node.parent.data, data, r.address);
              node.parent.insertBefore({ data }, node);
            });
            if (p.retainers_end) {
              node.parent.childNodes.pop();
            } else {
              rawdata.retainersCurrent = p.retainers_current;
              rawdata.retainersLeft = p.retainers_left;
            }
            vm.loadMoreStatus = false;
          }).catch(err => vm.$message.error(err));
      }
    },
    watch: {
      rootid() {
        var root = this.node.childNodes[0];
        root.childNodes = [];
        root.expanded = false;
        root.isLeaf = false;
        root.loaded = false;
        this.formatNode(this.nodeData, null, root.data);
      }
    }
  };
  if (typeof Devtoolx === 'undefined') window.Devtoolx = {};
  Devtoolx.TreeRetainers = TreeRetainers;
})();