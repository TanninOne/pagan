let pagan = require('./build/Release/node-pagan')
// let pagan = require('./build/RelWithDebInfo/node-pagan')
// let pagan = require('./build/Debug/node-pagand')

const { inspect } = require('util');

pagan.DynObject.prototype[inspect.custom] = function(depth, options) {
  return `DynObject {${this.keys.join(', ')}}`;
};

const proxyHandler = {
  enumerate(target) {
    if (target instanceof pagan.DynObject) {
      return target.keys;
    }
    return target.keys();
  },
  ownKeys(target) {
    return this.enumerate(target);
  },
  get(target, prop, receiver) {
    if (prop === '__deproxy') {
      return target;
    }
    let res = target[prop];
    if ((target instanceof pagan.DynObject) && target.keys.includes(prop)) {
      res = target.get(prop);
    }
    if ((typeof(res) === 'object') && !Buffer.isBuffer(res)) {
      return new Proxy(res, proxyHandler);
    } else if (res.apply !== undefined) {
      return function(...args) {
        return res.apply(target, args);
      }
    } else {
      return res;
    }
  },
  set(target, prop, value) {
    throw new Error('can\'t assign to object');
  },
}

const orig = pagan.Parser.getObject;
pagan.Parser.prototype['getObject'] = function(type, idx) {
  return new Proxy(orig(type, idx), proxyHandler);
}

function wrap(dynobj) {
  return new Proxy(dynobj, proxyHandler);
}

class Parser {
	constructor(spec) {
		this.mWrappee = new pagan.Parser(spec);
	}

	addFileStream(filePath) {
		this.mWrappee.addFileStream(filePath);
	}

	getType(typeName) {
		return this.mWrappee.getType(typeName);
	}

	getObject(type, offset) {
		return wrap(this.mWrappee.getObject(type, offset));
	}

	write(filePath, obj) {
		this.mWrappee.write(filePath, obj.__deproxy);
	}
	
}

module.exports = {
	Parser,
};
