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
    return 'urmom';
  },
  prepareForCommit: (...args) => {
    return null;
  },
  resetAfterCommit: (...args) => {},
  // resolveUpdatePriority: (...args) => {
  //   return () => 0;
  // },
  getChildHostContext: (...args) => {
    return 'urchild';
  },
  shouldSetTextContent(type, props) {
    const childrenType = typeof props.children;
    if (childrenType === 'string' || childrenType == 'number') return true;
    return false;
  },
  createTextInstance(text, _rootContainerInstance, _hostContext) {
    return {
      type: 'text',
      text,
    };
  },
  createInstance(type, props, rootContainerInstance, _hostContext) {
    const elementProps = { ...props };
    if (typeof elementProps.children === 'string') {
      elementProps.children = [{ type: 'text', text: elementProps.children }];
    } else {
      elementProps.children = [];
    }
    const element = { type, ...elementProps };
    return element;
  },
  detachDeletedInstance(...args) {
    return null;
  },
  removeChild(parentInstance, child) {
    const index = parentInstance.children.indexOf(child);
    if (index !== -1) {
      parentInstance.children.splice(index, 1);
    }
  },
  appendInitialChild(parentInstance, child) {
    parentInstance.children.push(child);
  },
  finalizeInitialChildren(...args) {
    return true;
  },
  clearContainer(rootContainerInstance) {
    rootContainerInstance.children = [];
  },
  appendChildToContainer(rootContainerInstance, child) {
    rootContainerInstance.children.push(child);
  },
  commitMount(instance, type, newProps) {},
  prepareUpdate(
    instance,
    type,
    oldProps,
    newProps,
    _rootContainerInstance,
    _hostContext
  ) {
    const { children, ...props } = newProps;
    return props;
  },
  commitTextUpdate(textInstance, oldText, newText) {
    textInstance.text = newText;
  },
  commitUpdate(instance, updatePayload, type, oldProps, newProps) {
    Object.assign(instance, updatePayload);
  },
};
const MukittyRenderer = Reconciler(hostConfig);

function renderTextElement(element) {
  if (element.type === 'text') {
    return element.text;
  }
  return element.children?.map(renderTextElement).join('') || '';
}
function renderElement(element) {
  let text = '';
  switch (element.type) {
    case 'window':
      mukitty.beginWindow('Mukitty React');
      for (let child of element.children) {
        renderElement(child);
      }
      mukitty.endWindow();
      break;
    case 'button':
      for (let child of element.children) {
        text += renderTextElement(child);
      }
      if (mukitty.button(text)) {
        element.onClick?.();
      }
      break;
    case 'row':
      if (element.items) {
        mukitty.layoutRow(element.items, element.height, ...element.widths);
      } else {
        mukitty.layoutRow();
      }
      for (let child of element.children) {
        renderElement(child);
      }
      break;
    case 'col':
      mukitty.beginColumn();
      for (let child of element.children) {
        renderElement(child);
      }
      mukitty.endColumn();
      break;
    case 'label':
      for (let child of element.children) {
        text += renderTextElement(child);
      }
      mukitty.label(text);
      break;
    case 'slider':
      const val = mukitty.slider(element.min, element.max, element.value);
      element.onChange?.(val);
      break;
    case 'checkbox':
      const checked = mukitty.checkbox(element.checked, element.label);
      element.onChange?.(checked);
      break;
    case 'input':
      const value = mukitty.textbox(element.value);
      element.onChange?.(value);
      break;
    case 'span':
      for (let child of element.children) {
        text += renderTextElement(child);
      }
      mukitty.text(text);
      break;
    case 'rect':
      mukitty.rect(element.color || 0xffffff);
      break;
    default:
      throw `Unknown element type: ${element.type}`;
  }
}

exports.render = (element, ttyMode = 'ghostty') => {
  const container = MukittyRenderer.createContainer({ type: 'window' }, 0);
  MukittyRenderer.updateContainer(element, container);

  mukitty.init(0, 0, ttyMode);
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
