
function currentTime() {
    return new Date().getTime();
}

function stringFormat(text) {
    var formatArgs = arguments;
    return text.replace(/\{(.+?)\}/g, function(str, p){
        return formatArgs[parseInt(p) + 1];
    });
}

var string_format = stringFormat;

function stringCompare(a, b) {
    if (a.name > b.name) {
        return 1;
    } else if (a.name < b.name) {
        return -1;
    }
    return 0;
}

function composeUrl(url, parameters) {
    var a = [];
    for (var name in parameters) {
        a.push(encodeURIComponent(name) + '=' + encodeURIComponent(parameters[name]));
    }
    if (a.length > 0) {
        return url + '?' + a.join('&');
    }
    return url;
}

function formatSize(size) {
    var UINTS = ["Bytes", "KB", "M", "G"]
    var unitIndex = 0
    while (size > 1024 && unitIndex < UINTS.length - 1) {
        size /= 1024.0
        unitIndex += 1
    }

    return size.toFixed(1).trimRight('0').trimRight('.') + " " + UINTS[unitIndex];
}

function assert(condition) {
    if (!condition) {
        throw 'Assert failed.';
    }
}

function objectValues(obj) {
    var a = [];
    for (var k in obj) {
        a.push(obj[k]);
    }

    return a;
}

function objectToList(obj) {
    var a = [];
    for (var k in obj) {
        a.push([k, obj[k]]);
    }

    return a;
}

function findInArray(arr, filterFunc) {
    for (var i = 0; i < arr.length; i++) {
        if (filterFunc(arr[i])) {
            return arr[i];
        }
    }
}

function isElementInTree(node, parent) {
    while (node) {
        if (parent === node) {
            return true;
        }

        node = node.parentElement;
    }
}

function newPopper(idPopView, toggleBtn, placement) {
    var popElement = document.getElementById(idPopView);
    var refElement = document.getElementById(toggleBtn);

    var popper = new Popper(refElement,
        popElement, {
            placement: placement || 'bottom',
            modifiers: {
                arrow: {
                    element: '.arrow'
                },
            },
        });

    document.addEventListener('mousedown', function(e) {
        if (popElement.style.display != 'none' &&
            !isElementInTree(e.target, refElement) &&
            !isElementInTree(e.target, popElement)) {
            popElement.style.display = 'none';
        }
    });

    return {
        toggle: function () {
            popper.update();
            $(popElement).toggle();
        },
        setToggleRefElement: function (newRefElement) {
            refElement = newRefElement;
            popper.reference = newRefElement;
        },
    };
}

function validateEmail(email) {
    var re = /^(([^<>()\[\]\\.,;:\s@"]+(\.[^<>()\[\]\\.,;:\s@"]+)*)|(".+"))@((\[[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\])|(([a-zA-Z\-0-9]+\.)+[a-zA-Z]{2,}))$/;
    return re.test(String(email).toLowerCase());
}

function createPaginationNumbers(pageCount, curPage) {
    function createPageNumbers(start, end) {
        for (var i = start; i <= end; i++) {
            pages.push([i, i]);
        }
    }

    if (pageCount <= 0) {
        pageCount = 1;
    }

    if (curPage < 1) {
        curPage = 1;
    } else if (curPage >= pageCount) {
        curPage = pageCount;
    }

    // Previous button
    var pages = [];
    if (curPage == 1) {
        pages.push(['<<', '']);
    } else {
        pages.push(['<<', curPage - 1]);
    }

    if (pageCount <= 11) {
        createPageNumbers(1, pageCount);
    } else {
        // 1, 2, 3, ... curPage - 1, curPage, curPage + 1, ... pageCount - 3, pageCount - 2, pageCount - 1
        if (curPage <= 5) {
            createPageNumbers(1, curPage + 1);
            pages.push(['...', curPage + 2]);
            createPageNumbers(pageCount - 2, pageCount);
        } else {
            createPageNumbers(0, 2);
            pages.push(['...', 3]);

            if (pageCount - curPage <= 6) {
                createPageNumbers(curPage - 1, pageCount);
            } else {
                // Middle pages
                createPageNumbers(curPage - 1, curPage + 1);

                pages.push(['...', pageCount - 3]);

                createPageNumbers(pageCount - 2, pageCount);
            }
        }
    }

    // Next button
    if (curPage == pageCount) {
        pages.push(['>>', '']);
    } else {
        pages.push(['>>', curPage + 1]);
    }

    return pages;
}

function decodeHtml(html) {
    const textArea = document.createElement('textarea');
    textArea.innerHTML = html;
    return textArea.value;
}