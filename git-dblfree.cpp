
#include <iostream>
#include <string>
#include <set>
#include <sstream>
#include <cstring>
#include <git2.h>

using namespace std;


static int processedCount = 0;
static set<string> knownCommits;
struct TreeWalkState {
    git_repository *repo;
    git_commit *newestCommit;
};



/**
 * Convert git_oid to string.
 */
string idToString(const git_oid *oid) {
    char id[40];
    git_oid_fmt(id, oid);
    return string(id, 40);
}


string getLastGitError() {
    stringstream ss;
    const git_error *e = giterr_last();
    ss << e->klass << ": " << e->message;
    return ss.str();
}


/**
 * Get the HEAD commit.
 */
git_commit* getHead(git_repository *repo) {
    int rc;
    git_object *obj;

    if((rc = git_revparse_single(&obj, repo, "HEAD"))) {
        cerr << "failed to get HEAD: " << rc << "\n";
        return nullptr;
    }

    return (git_commit*)obj;
}

/**
 * Build tree of files.
 */
int treeMetricsCallback(const char *root, const git_tree_entry *entry, void *payload) {
    if(git_tree_entry_type(entry) == GIT_OBJ_BLOB) {
        TreeWalkState *state = (TreeWalkState*)payload;
        string path = root;
        int rc;
        git_blame *blame;
        const git_blame_hunk *hunk;
        uint32_t hunkCount;
        git_blame_options opts = GIT_BLAME_OPTIONS_INIT;

        memcpy(&opts.newest_commit, git_commit_id(state->newestCommit), sizeof(git_oid));
        path += git_tree_entry_name(entry);
        
        cout << "  blame " << path << ": " << flush;
        
        
        rc = git_blame_file(&blame, state->repo, path.c_str(), &opts);
        if(rc) {
            cout << "failed to create blame: " << getLastGitError() << "\n";
            return 0;
        }
        
        hunkCount = git_blame_get_hunk_count(blame);
        for(uint32_t i = 0; i < hunkCount; ++i) {
            hunk = git_blame_get_hunk_byindex(blame, i);
        }
        
        cout << "done\n";
        
        git_blame_free(blame);
    }
    
    return 0;
}

/**
 * Recursively iterate to the bottom of the commit history then begin to perform
 * a blame on each file in the commit's tree.
 */
void processCommit(git_commit *commit) {
    int parentCount;
    git_tree *tree;
    string id = idToString(git_commit_id(commit));
    TreeWalkState state;
    int rc;
    
    // First, check if we've seen this commit before and, if we have, return
    // immediately.
    auto cached = knownCommits.insert(id);
    if(!cached.second) {
        // We've already seen this commit, bail
        return;
    }
    
    // We process the bottom commit first, so handle parents first before
    // processing the input commit.
    parentCount = git_commit_parentcount(commit);
    for(int i = 0; i < parentCount; ++i) {
        git_commit *parent;
        
        rc = git_commit_parent(&parent, commit, i);
        if(rc) {
            cerr << "Failed to get parent of commit " << id << ": " << rc << "\n";
        } else {
            processCommit(parent);
            git_commit_free(parent);
        }
    }
    
    rc = git_commit_tree(&tree, commit);
    if(rc) {
        cerr << "Failed to get tree of commit " << id << ": " << rc << "\n";
        return;
    }
    
    cerr << "processing commit: " << id << " (" << processedCount << " / "
        << knownCommits.size() << ")\n";
    
    state.repo = git_commit_owner(commit);
    state.newestCommit = commit;
    git_tree_walk(tree, GIT_TREEWALK_PRE, treeMetricsCallback, &state);
    git_tree_free(tree);
    
    ++processedCount;
}




int main(int argc, char **argv) {
    char *repoPath;
    int rc;
    git_repository *repo;
    git_commit *head;
    
    if(argc != 2) {
        cerr << "usage: " << argv[0] << " Repo_Path\n";
        return 1;
    }
    
    git_libgit2_init();
    
    repoPath = argv[1];
    rc = git_repository_open_ext(&repo, repoPath, 0, nullptr);
    if(rc) {
        cerr << "failed to open repo: " << rc << "\n";
        git_libgit2_shutdown();
        return -1;
    }
    
    head = getHead(repo);
    if(!head) {
        git_repository_free(repo);
        git_libgit2_shutdown();
        return -1;
    }
    
    cout << "found head: " << idToString(git_commit_id(head)) << "\n";;
    processCommit(head);
    git_commit_free(head);
    
    return 0;
}