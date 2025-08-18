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
  appendChild(parentInstance, child) {
    parentInstance.children.push(child);
  },
  insertInContainerBefore(rootContainerInstance, child, beforeChild) {
    const index = rootContainerInstance.children.indexOf(beforeChild);
    if (index !== -1) {
      rootContainerInstance.children.splice(index, 0, child);
    } else {
      rootContainerInstance.children.push(child);
    }
  },
  removeChildFromContainer(rootContainerInstance, child) {
    const index = rootContainerInstance.children.indexOf(child);
    if (index !== -1) {
      rootContainerInstance.children.splice(index, 1);
    }
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
  switch (element.type) {
    case 'window':
      {
        mukitty.beginWindow(element.id ?? 'root');
        for (let child of element.children) {
          renderElement(child);
        }
        mukitty.endWindow();
      }
      break;
    case 'modal':
      {
        const open = mukitty.beginWindow(
          element.title,
          element.top,
          element.left,
          element.width,
          element.height
        );
        if (open) {
          for (let child of element.children) {
            renderElement(child);
          }
          mukitty.endWindow();
        } else {
          element.onClose?.();
        }
      }
      break;
    case 'button':
      {
        let text = '';
        for (let child of element.children) {
          text += renderTextElement(child);
        }
        if (mukitty.button(text)) {
          element.onClick?.();
        }
      }
      break;
    case 'row':
      {
        const widths = element.widths ?? [];
        mukitty.layoutRow(element.height, ...widths);
        for (let child of element.children) {
          renderElement(child);
        }
        mukitty.layoutRow();
      }
      break;
    case 'col':
      {
        mukitty.beginColumn();
        for (let child of element.children) {
          renderElement(child);
        }
        mukitty.endColumn();
      }
      break;
    case 'label':
      {
        let text = '';
        for (let child of element.children) {
          text += renderTextElement(child);
        }
        mukitty.label(text);
      }
      break;
    case 'slider':
      {
        const val = mukitty.slider(element.min, element.max, element.value);
        element.onChange?.(val);
      }
      break;
    case 'checkbox':
      {
        const checked = mukitty.checkbox(element.checked, element.label);
        element.onChange?.(checked);
      }
      break;
    case 'input':
      {
        const { text, submit } = mukitty.textbox(element.value);
        element.onChange?.(text);
        if (submit) {
          element.onSubmit?.(text);
        }
      }
      break;
    case 'text':
      {
        let text = '';
        for (let child of element.children) {
          text += renderTextElement(child);
        }
        mukitty.text(text);
      }
      break;
    case 'rect':
      {
        mukitty.rect(element.color || 0xffffff);
      }
      break;
    case 'tree':
      {
        const open = mukitty.beginTreeNode(element.title, element.startOpened);
        if (!open) {
          element.onClose?.();
          return;
        }
        for (let child of element.children) {
          renderElement(child);
        }
        mukitty.endTreeNode();
      }
      break;
    case 'header':
      {
        const open = mukitty.header(element.title, element.startOpened);
        if (!open) {
          element.onClose?.();
          return;
        }
        for (let child of element.children) {
          renderElement(child);
        }
      }
      break;
    case 'panel':
      {
        mukitty.beginPanel(element.title);
        for (let child of element.children) {
          renderElement(child);
        }
        mukitty.endPanel();
      }
      break;
    default:
      throw `Unknown element type: ${element.type}`;
  }
}

exports.render = (element) => {
  const container = MukittyRenderer.createContainer({ type: 'window' }, 0);
  MukittyRenderer.updateContainer(element, container);

  mukitty.init();
  while (true) {
    const stop = mukitty.handleInputs();
    if (stop) break;
    mukitty.begin();
    renderElement(container.containerInfo);
    mukitty.end();
  }
  mukitty.close();
};
