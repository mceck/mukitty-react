const React = require('react');
const Reconciler = require('react-reconciler');
const mukitty = require('./build/Release/mukitty.node');

const TRACE = false; // Set to true for debugging

const hostConfig = {
  noTimeout: -1,
  isPrimaryRenderer: true,
  supportsMutation: true,
  supportsPersistence: false,
  supportsHydration: false,
  getRootHostContext: (...args) => {
    if (TRACE) console.log('getRootHostContext', args);
    return 'urmom';
  },
  prepareForCommit: (...args) => {
    if (TRACE) console.log('prepareForCommit', args);
    return null;
  },
  resetAfterCommit: (...args) => {
    if (TRACE) console.log('resetAfterCommit', args);
  },
  // resolveUpdatePriority: (...args) => {
  //   if (TRACE) console.log('resolveUpdatePriority', args);
  //   return () => 0;
  // },
  getChildHostContext: (...args) => {
    if (TRACE) console.log('getChildHostContext', args);
    return 'urchild';
  },
  shouldSetTextContent(type, props) {
    if (TRACE) console.log('shouldSetTextContent', type, props);
    const childrenType = typeof props.children;
    if (childrenType === 'string' || childrenType == 'number') return true;
    return false;
  },
  createTextInstance(text, _rootContainerInstance, _hostContext) {
    if (TRACE) console.log('createTextInstance');
    return {
      type: 'text',
      text,
    };
  },
  createInstance(type, props, rootContainerInstance, _hostContext) {
    if (TRACE) console.log('createInstance');
    const elementProps = { ...props };
    if (typeof elementProps.children === 'string') {
      elementProps.children = [{ type: 'text', text: elementProps.children }];
    } else {
      elementProps.children = [];
    }
    const element = { type, ...elementProps };
    return element;
  },
  appendInitialChild(parentInstance, child) {
    if (TRACE) console.log('appendInitialChild');
    parentInstance.children.push(child);
  },
  finalizeInitialChildren(...args) {
    if (TRACE) console.log('finalizeInitialChildren', args);
    return true;
  },
  clearContainer(rootContainerInstance) {
    if (TRACE) console.log('clearContainer', rootContainerInstance);
    rootContainerInstance.children = [];
  },
  appendChildToContainer(rootContainerInstance, child) {
    if (TRACE)
      console.log('appendChildToContainer', rootContainerInstance, child);
    rootContainerInstance.children.push(child);
  },
  commitMount(instance, type, newProps) {
    if (TRACE) console.log('commitMount', instance, type, newProps);
  },
  prepareUpdate(
    instance,
    type,
    oldProps,
    newProps,
    _rootContainerInstance,
    _hostContext
  ) {
    if (TRACE) console.log('prepareUpdate');
    const changes = {
      props: [],
      style: [],
    };
    for (let key in { ...oldProps, ...newProps }) {
      if (oldProps[key] !== newProps[key]) {
        changes.props.push(key);
      }
    }
    for (let key in { ...oldProps.style, ...newProps.style }) {
      if (oldProps.style[key] !== newProps.style[key]) {
        changes.style.push(key);
      }
    }
    // const updatePayload = changes.props.length || changes.style.length ? { changes } : null;
    return changes;
  },
  commitTextUpdate(textInstance, oldText, newText) {
    textInstance.text = newText;
    if (TRACE) console.log('commitTextUpdate', textInstance, oldText, newText);
  },
  commitUpdate(instance, updatePayload, type, oldProps, newProps) {
    if (TRACE) console.log('commitUpdate', args);
    for (let prop of updatePayload.props) {
      if (prop !== 'children') {
        instance[prop] = newProps[prop];
      }
    }
  },
};
const MukittyRenderer = Reconciler(hostConfig);

function renderTextElement(element) {
  if (element.type === 'text') {
    return element.text;
  }
  if (TRACE) console.log(element);
  throw 'Not text!';
}
function renderElement(element) {
  switch (element.type) {
    case 'window':
      mukitty.beginWindow('Mukitty React');
      for (let child of element.children) {
        renderElement(child);
      }
      mukitty.endWindow();
      break;
    case 'button':
      let label = '';
      for (let child of element.children) {
        label += renderTextElement(child);
      }
      if (mukitty.button(label)) {
        element.onClick?.();
      }
      break;
    case 'layout':
      mukitty.layoutRow(element.items, element.height, ...element.widths);
      for (let child of element.children) {
        renderElement(child);
      }
      break;
    case 'label':
      // console.log('render label', element);
      // process.exit(1);
      let text = '';
      for (let child of element.children) {
        text += renderTextElement(child);
      }
      mukitty.label(text);
      break;
    case 'slider':
      if (TRACE) console.log('render slider', element);
      const val = mukitty.slider(element.min, element.max, element.value);
      element.onChange?.(val);
      break;
    case 'checkbox':
      if (TRACE) console.log('render checkbox', element);
      const checked = mukitty.checkbox(element.checked, element.label);
      element.onChange?.(checked);
      break;
    case 'input':
      if (TRACE) console.log('render input', element);
      const value = mukitty.textbox(element.value);
      element.onChange?.(value);
      break;
    default:
      throw 'TODO';
  }
}

exports.render = (element) => {
  const container = MukittyRenderer.createContainer({ type: 'window' }, 0);
  MukittyRenderer.updateContainer(element, container);

  mukitty.init();
  while (true) {
    const stop = mukitty.updateInput();
    if (stop) break;
    mukitty.clearBackground();
    mukitty.begin();
    renderElement(container.containerInfo);
    mukitty.end();
  }
  mukitty.close();
};
