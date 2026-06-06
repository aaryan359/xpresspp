import { defineConfig } from 'vitepress'

export default defineConfig({
  title: 'Xpress++',
  description: 'Dragon-speed, Express-style C++ web framework — complete documentation.',
  cleanUrls: true,
  appearance: 'auto',
  lastUpdated: true,
  markdown: {
    lineNumbers: true,
    theme: {
      light: 'github-light',
      dark:  'one-dark-pro'
    }
  },
  head: [
    ['meta', { name: 'theme-color', content: '#0d9488' }],
    ['link', { rel: 'preconnect', href: 'https://fonts.googleapis.com' }],
    ['link', { rel: 'preconnect', href: 'https://fonts.gstatic.com', crossorigin: '' }],
    ['link', { href: 'https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&family=JetBrains+Mono:wght@400;500;600&display=swap', rel: 'stylesheet' }],
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:title', content: 'Xpress++' }],
    ['meta', { property: 'og:description', content: 'Dragon-speed C++ web framework with Express-like simplicity.' }],
  ],
  themeConfig: {
    logo: { light: '/logo-light.svg', dark: '/logo-dark.svg' },
    siteTitle: 'Xpress++',
    search: {
      provider: 'local'
    },
    nav: [
      { text: 'Guide',     link: '/getting-started' },
      { text: 'Reference', link: '/request' },
      { text: 'CLI',       link: '/cli' },
      {
        text: 'GitHub',
        link: 'https://github.com/your-org/xpresspp',
        target: '_blank'
      }
    ],
    sidebar: [
      {
        text: '🚀 Getting Started',
        items: [
          { text: 'Introduction',      link: '/'                   },
          { text: 'Installation',      link: '/getting-started'    },
          { text: 'Project structure', link: '/project-structure'  },
          { text: 'Dependencies',      link: '/dependencies'       },
        ]
      },
      {
        text: '⚡ Core Framework',
        items: [
          { text: 'Routing',           link: '/routing'           },
          { text: 'Request',           link: '/request'           },
          { text: 'Response',          link: '/response'          },
          { text: 'Middleware',        link: '/middleware'         },
          { text: 'Error handling',    link: '/error-handling'    },
          { text: 'Configuration',     link: '/configuration'     },
        ]
      },
      {
        text: '🧱 Built-in Middleware',
        items: [
          { text: 'CORS',              link: '/cors'              },
          { text: 'Authentication',    link: '/authentication'    },
          { text: 'Rate limiting',     link: '/rate-limit'        },
          { text: 'Security headers',  link: '/security'          },
          { text: 'CSRF protection',   link: '/csrf'              },
          { text: 'Session',           link: '/session'           },
          { text: 'Request ID',        link: '/request-id'        },
          { text: 'Body limit',        link: '/body-limit'        },
          { text: 'Validation',        link: '/validation'        },
          { text: 'Logging',           link: '/logging'           },
        ]
      },
      {
        text: '📦 Features',
        items: [
          { text: 'Static files',      link: '/static-files'      },
          { text: 'File uploads',      link: '/file-uploads'      },
          { text: 'JSON',              link: '/json'              },
          { text: 'Cookies',           link: '/cookies'           },
        ]
      },
      {
        text: '🔧 Tooling',
        items: [
          { text: 'xp CLI',            link: '/cli'               },
          { text: 'Next steps',        link: '/next-steps'        },
        ]
      }
    ],
    editLink: {
      pattern: 'https://github.com/your-org/xpresspp/edit/main/docs/:path',
      text: 'Edit this page'
    },
    footer: {
      message: 'Built with ❤️ on top of Drogon, the world\'s fastest C++ HTTP framework.',
      copyright: 'Copyright © 2026 Xpress++'
    },
    socialLinks: [
      { icon: 'github', link: 'https://github.com/your-org/xpresspp' }
    ]
  }
})
