
taf::Int32 DocInfoImp::GetDocDataRelated(const TencareBaike::CommonHeader & header,
                                         const TencareBaike::GetDocDataRelatedReq & req,
                                         TencareBaike::GetDocDataRelatedResp &resp,
                                         taf::JceCurrentPtr current) {
  PrintParams(__func__, header, req);
  RemoteLog(__func__, header, req);
  string docid            = req.docid;
  int type                = req.type;
  string disease          = req.disease;
  string treerootid       = req.treerootid;
  string treeid           = req.treeid;
  string tagid            = req.tagid;
  string group_id         = req.group_id;
  string disdocid         = req.disdocid;
  string dislisttabid     = req.dislisttabid;
  string discatalogtabid  = req.discatalogtabid;
  int version             = req.version;

  // 参数校验
  if (docid.empty()) {
    TLOGDEBUGEXT("check docid empty, return");
    return TencareBaike::E_SUCCESS;
  }

  // 记录最近浏览
  if (mEnableDocRecent > 0) {
    mDataProxyHelper->AddToDocRecent(header.uin, docid);
    AsyncDoUserDocRelationGrpc(header, "yidian", "doc_recent_read", docid, header.uin, 1, 3600*24*20);
  } else {
    TLOGDEBUGEXT("skip add to doc rencent ");
  }

  string docs_str;
  int ret = DCacheGetString(docid, docs_str);
  if (ret != 0) {
    TLOGERROR("DCacheGetString fail, ret: "<<ret<<", key: "<<docid<<endl);
    return TencareBaike::E_NO_RECORD;
  }
  TLOGDEBUG("docid: "<<docid<<", docinfo: "<<docs_str<<endl);
  Document dDocInfo;
  if (dDocInfo.Parse(docs_str.c_str()).HasParseError()) {
    TLOGDEBUG("parse docs_str fail: "<<docs_str<<endl);
    return TencareBaike::E_JSON_PARSE_ERROR;
  }

  vector<int> vtDid;                  // 文章疾病ID
  vector<string> vtDocDiseases;       // 文章疾病
  do {
    vector<string> vtTreatment;
    vector<string> vtTag;
    if (dDocInfo.HasMember("theme") && dDocInfo["theme"].IsArray()) {
      Value& theme = dDocInfo["theme"];

      std::vector<std::string> docids;
      for (size_t i = 0; i < theme.Size(); ++i) {
        if (theme[i].HasMember("docid") && theme[i]["docid"].IsString()) {
          docids.push_back(theme[i]["docid"].GetString());
        }
        if (theme[i].HasMember("recommend") && theme[i]["recommend"].IsArray()) {
          Value& recommend = theme[i]["recommend"];
          for (size_t j= 0; j < recommend.Size(); ++j) {
            docids.push_back(recommend[j].GetString());
          }
        }
      }
      std::map<std::string, Tid_DocInfo> docid_tid_docinfo;

      {
        vector<SKeyValue> vtValue;
        DCacheGetStringBatch(docids, vtValue);
        for (size_t i = 0; i < vtValue.size(); i++) {
          if (vtValue.at(i).ret == 0) {
            string docid   = vtValue.at(i).keyItem;
            string docinfo = vtValue.at(i).value;
            Document dDocInfo;
            if (!dDocInfo.Parse(docinfo.c_str()).HasParseError() && dDocInfo.HasMember("index")
                && dDocInfo["index"].HasMember("tid") && dDocInfo["index"]["tid"].IsInt() ) {
              Tid_DocInfo oDocInfo;
              if (dDocInfo["index"]["tid"].GetInt() == 4) {
                oDocInfo.tid = 4;
                oDocInfo.tid4.title = dDocInfo["index"]["title"].GetString();
                oDocInfo.tid4.docid = dDocInfo["index"]["docid"].GetString();
              } else if (dDocInfo["index"]["tid"].GetInt() == 10) {
                oDocInfo.tid = 10;
                oDocInfo.tid10.title = dDocInfo["index"]["title"].GetString();
                oDocInfo.tid10.docid = dDocInfo["index"]["docid"].GetString();
              } else if (dDocInfo["index"]["tid"].GetInt() == 25) {
                oDocInfo.tid = 25;
                oDocInfo.tid25.title = dDocInfo["index"]["title"].GetString();
                oDocInfo.tid25.docid = dDocInfo["index"]["docid"].GetString();
              }
              if (oDocInfo.tid != 0) {
                docid_tid_docinfo[docid] = oDocInfo;
              }
            } else {
              TLOGERROR("docinfo value error, docinfo: "<<docinfo<<endl);
            }
          }
        }
      }

      for (size_t i = 0; i < theme.Size(); ++i) {
        ThemeDoc theme_doc;
        if (theme[i].HasMember("docid") && theme[i]["docid"].IsString()) {
          string sDocIdTemp = theme[i]["docid"].GetString();
          if (docid_tid_docinfo.find(sDocIdTemp) != docid_tid_docinfo.end()) {
            theme_doc.doc = docid_tid_docinfo[sDocIdTemp];
          } else {
            TLOGDEBUGEXT("ignore theme_doc.doc docid: "<<sDocIdTemp);
          }
        }
        if (theme[i].HasMember("recommend") && theme[i]["recommend"].IsArray()) {
          Value& recommend = theme[i]["recommend"];
          for (size_t j= 0; j < recommend.Size(); ++j) {
            string sDocIdTemp = recommend[j].GetString();
            if (docid_tid_docinfo.find(sDocIdTemp) != docid_tid_docinfo.end()) {
              theme_doc.recomment_docs.push_back(docid_tid_docinfo[sDocIdTemp]);
            } else {
            TLOGDEBUGEXT("ignore theme_doc.recomment_docs docid: "<<sDocIdTemp);
          }
          }
        }
        if (theme_doc.doc.tid != 0 && !theme_doc.recomment_docs.empty()) {
          resp.theme_docs.push_back(theme_doc);
        }
      }
    }

    std::vector<std::string> related_disease_docids;
    std::vector<std::string> related_disease_names;
    // 获取文章相关疾病
    if (dDocInfo.HasMember("related_disease") && dDocInfo["related_disease"].IsArray()) {
      for (size_t j = 0; j < dDocInfo["related_disease"].Size(); ++j) {
        if (dDocInfo["related_disease"][j].HasMember("docid") && dDocInfo["related_disease"][j]["docid"].IsString() &&
            dDocInfo["related_disease"][j].HasMember("name") && dDocInfo["related_disease"][j]["name"].IsString()) {
          related_disease_docids.push_back(dDocInfo["related_disease"][j]["docid"].GetString());
          related_disease_names.push_back(dDocInfo["related_disease"][j]["name"].GetString());
        }
      }
    }

    if (!related_disease_docids.empty()) {
      std::map<std::string, DocidInfo> docid_docidinfo;
      GetDocidInfos(related_disease_docids, &docid_docidinfo);

      std::map<std::string, std::string> docid_single_abs;
      vector<SKeyValue> vtValue;
      vector<std::string> vtKey;
      DCacheGetStringBatch(related_disease_docids, vtValue);
      map<string, string> mapDocInfo;
      for (size_t i = 0; i < vtValue.size(); i++) {
        if (vtValue.at(i).ret == 0) {
          string docid  = vtValue.at(i).keyItem;
          string docs_str = vtValue.at(i).value;

          Document dDocInfo;
          dDocInfo.Parse(docs_str.c_str());
          if (dDocInfo.HasParseError()) {
            continue;
          }

          if (dDocInfo.HasMember("single_abs") && dDocInfo["single_abs"].IsString()) {
            docid_single_abs[docid] = dDocInfo["single_abs"].GetString();
          }
        }
      }

      map<string, DiseaseItem> mapDiseaseInfoList;
      GetDiseaseInfoList(related_disease_names, mapDiseaseInfoList);
      for (size_t i = 0; i < related_disease_docids.size() && i < related_disease_names.size(); ++i) {
        std::string& docid    = related_disease_docids[i];
        std::string& disease  = related_disease_names[i];
        std::map<std::string, DocidInfo>::iterator docidinfo_it = docid_docidinfo.find(docid);
        if (docidinfo_it == docid_docidinfo.end()) continue;
        //过滤非疾病
        if (docidinfo_it->second.ctype != 0) continue;

        RelateKnowledge related_knowledge;
        related_knowledge.title = disease;
        related_knowledge.abs = docid_single_abs[docid];
        related_knowledge.docid = docid;
        if (mapDiseaseInfoList.find(disease) != mapDiseaseInfoList.end()) {
          related_knowledge.type      = mapDiseaseInfoList[disease].type;
          related_knowledge.released  = mapDiseaseInfoList[disease].released;
          related_knowledge.h5url     = mapDiseaseInfoList[disease].h5url;
          related_knowledge.wxaurl    = mapDiseaseInfoList[disease].wxaurl;
        }
        resp.related_diseases.push_back(related_knowledge);
      }
      TLOGDEBUG("related_diseases_size=" << resp.related_diseases.size()<<endl);
      for (size_t i = 0; i < resp.related_diseases.size(); ++i) {
        TLOGDEBUG("knowledge[" << i << "]=" << resp.related_diseases[i].writeToJsonString());
      }
    }

    if (dDocInfo.HasMember("index") && dDocInfo["index"].HasMember("tid")) {
      // 获取标签信息
      DocData docData;
      Value& vDocData = dDocInfo["index"];
      int tid = vDocData["tid"].GetInt();
      if (tid == 4) {
        Tid4 tid4;
        Json2Jce::Tid4(vDocData, tid4);
        for (vector<string>::const_iterator iter = tid4.op_keyword.begin(); iter != tid4.op_keyword.end(); ++iter) {
          vtTag.push_back(*iter);
        }
      } else if (tid == 10) {
        Tid10 tid10;
        Json2Jce::Tid10(vDocData, tid10);
        for (vector<string>::const_iterator iter = tid10.op_keyword.begin(); iter != tid10.op_keyword.end(); ++iter) {
          vtTag.push_back(*iter);
        }
      } else if (tid == 25) {
        Tid25 tid25;
        Json2Jce::Tid25(vDocData, tid25);
        for (vector<string>::const_iterator iter = tid25.op_keyword.begin(); iter != tid25.op_keyword.end(); ++iter) {
          vtTag.push_back(*iter);
        }
      }
    }
    if (dDocInfo.HasMember("data") && dDocInfo["data"].HasMember("tid")) {
      // 获取标签信息
      vector<string> vtRelateTag;
      if (dDocInfo.HasMember("keyword") && dDocInfo["keyword"].IsArray()) {
        for(SizeType i = 0; i < dDocInfo["keyword"].Size(); i++) {
          if (dDocInfo["keyword"][i].IsObject()) {
            Value& tagsObj = dDocInfo["keyword"][i];
            if (tagsObj.HasMember("name") && tagsObj["name"].IsString()) {
              string sNameTemp = tagsObj["name"].GetString();
              vtRelateTag.push_back(sNameTemp);
            }
          }
        }
      }

      // 获取疾病信息
      if (docid.substr(0,2) == "em") {
        vtDocDiseases.push_back("急救");
      }

      DocData docData;
      Value& vDocData = dDocInfo["data"];
      int tid = vDocData["tid"].GetInt();
      if (tid == 6) {
        docData.tid = tid;
        Json2Jce::Tid6(vDocData, docData.tid6);
        for (vector<Tid_DiseaseItem>::const_iterator iter = docData.tid6.diseases.begin(); iter != docData.tid6.diseases.end(); ++iter) {
          vtDocDiseases.push_back(iter->name);
        }
        vtTreatment = docData.tid6.sole_recruit.treatment;
      } else if (tid == 17) {
        docData.tid = tid;
        Json2Jce::Tid17(vDocData, docData.tid17);
        for (vector<Tid_DiseaseItem>::const_iterator iter = docData.tid17.diseases.begin(); iter != docData.tid17.diseases.end(); ++iter) {
          vtDocDiseases.push_back(iter->name);
        }
      } else if (tid == 24) {
        docData.tid = tid;
        Json2Jce::Tid24(vDocData, docData.tid24);
        for (vector<Tid_DiseaseItem>::const_iterator iter = docData.tid24.diseases.begin(); iter != docData.tid24.diseases.end(); ++iter) {
          vtDocDiseases.push_back(iter->name);
        }
      } else if (tid == 26) {
        docData.tid = tid;
        Json2Jce::Tid26(vDocData, docData.tid26);
        for (vector<Tid_DiseaseItem>::const_iterator iter = docData.tid26.diseases.begin(); iter != docData.tid26.diseases.end(); ++iter) {
          vtDocDiseases.push_back(iter->name);
        }
      }

      // 获取疾病did
      // if (!vtDocDiseases.empty()) {
      //   vector<DCDiseaseInfo> vtDiseaseInfo;
      //   GetDiseaseListFilter oFilter;
      //   for (vector<string>::const_iterator iter = vtDocDiseases.begin(); iter != vtDocDiseases.end(); ++iter) {
      //     oFilter.diseases.push_back(*iter);
      //   }
      //   mDataProxyHelper->GetDiseaseList(oFilter, vtDiseaseInfo);
      //   for (vector<DCDiseaseInfo>::const_iterator iter = vtDiseaseInfo.begin(); iter != vtDiseaseInfo.end(); ++iter) {
      //     vtDid.push_back(iter->did);
      //   }
      // }
      // 获取文章疾病+关联疾病
      vector<string> vtDisease;
      for (vector<string>::const_iterator iter = vtDocDiseases.begin(); iter != vtDocDiseases.end(); ++iter) {
        vtDisease.push_back(*iter);
      }
      // 此步骤已作废
      // mDataProxyHelper->GetRelatedDiseases(vtDid, vtDisease);

      // 获取用户关注疾病
      // vector<pair<std::string, taf::Int64> > vDiseaseTimePairTmp;
      // ret = mDataProxyHelper->GetFollowList(header.uin, 0, 1000000000, vDiseaseTimePairTmp);
      // for (size_t i=0; i < vDiseaseTimePairTmp.size(); i++) {
      //   setFollowDis.insert(vDiseaseTimePairTmp[i].first);
      // }
      set<string> setFollowDis;
      GetUserDocRelationResp getUserDocRelationRsp;
      ret = GetUserDocRelationGrpc(header, "yidian", "following_disease", header.uin, &getUserDocRelationRsp);
      for (int i=getUserDocRelationRsp.list.size()-1; i >= 0; i--) {
        setFollowDis.insert(getUserDocRelationRsp.list[i].doc);
      }
      for (vector<string>::const_iterator iter = vtDisease.begin(); iter != vtDisease.end(); ++iter) {
        string sDiseaseTemp = *iter;
        if (g_app.mDiseaseMap.find(sDiseaseTemp) != g_app.mDiseaseMap.end() &&
            g_app.mDiseaseMap[sDiseaseTemp].released == 1) {
          GetDiseaseListByCategoryRespListDiseases oDiseases;
          oDiseases.name = sDiseaseTemp;
          // 所属种类/科室/部位信息 暂时不需要
          oDiseases.released  = g_app.mDiseaseMap[sDiseaseTemp].released;
          oDiseases.type      = g_app.mDiseaseMap[sDiseaseTemp].type;
          resp.diseases.push_back(oDiseases);

          resp.docdiseases.push_back(g_app.mDiseaseMap[sDiseaseTemp]);
          // 获取疾病描述
          string summary;
          GetSummaryByDiseaseItem(header, sDiseaseTemp, summary);
          resp.docdiseases[resp.docdiseases.size()-1].summary = summary;
          // 获取用户是否关注疾病
          if (setFollowDis.find(sDiseaseTemp) != setFollowDis.end()) {
            resp.docdiseases[resp.docdiseases.size()-1].follow = 1;
            TLOGDEBUG("follow disesae: "<<sDiseaseTemp<<endl);
          } else {
            TLOGDEBUG("not follow disesae: "<<sDiseaseTemp<<endl);
          }
        } else {
          TLOGDEBUG("check disease not found or not released, disease: "<<sDiseaseTemp<<endl);
        }
      }

      // 系列视频
      vector<string> vtIgnoreDoc;
      if (tid == 17 && !docData.tid17.diseases.empty() && !docData.tid17.diseases[0].name.empty()) {
        vector<string> vtVideoRelateDocId;
        string sDiseaseTemp = docData.tid17.diseases[0].name;
        if (!disease.empty()) {
          sDiseaseTemp = disease;
        }
        string sVideoSetName = "视频聚合页_视频合集_"+sDiseaseTemp;
        string sVideoSetDocsStr;
        int ret = DCacheGetString(sVideoSetName, sVideoSetDocsStr);
        if (ret != 0) {
          TLOGERROR("DCacheGetString fail, ret: "<<ret<<", key: "<<sVideoSetName<<endl);
        } else {
          TLOGDEBUG("name: "<<sVideoSetName<<", docinfo: "<<sVideoSetDocsStr<<endl);
          Document dVideoSetDocs;
          if (!dVideoSetDocs.Parse(sVideoSetDocsStr.c_str()).HasParseError() && dVideoSetDocs.IsArray()) {
            vector<string> vtVideoSetDocId;
            vector<GetVideoSetFirst> vtVideoSetChildren;
            for (SizeType i = 0; i < dVideoSetDocs.Size(); i++) {
              Value& firstObj = dVideoSetDocs[i];
              GetVideoSetFirst oFirst;
              if (firstObj.HasMember("title") && firstObj["title"].IsString()) {
                oFirst.title = firstObj["title"].GetString();
              }
              if (firstObj.HasMember("children") && firstObj["children"].IsArray()) {
                for (SizeType j = 0; j < firstObj["children"].Size(); j++) {
                  Value& secondObj = firstObj["children"][j];
                  GetVideoSetSecond oSecond;
                  if (secondObj.HasMember("title") && secondObj["title"].IsString()) {
                    oSecond.title = secondObj["title"].GetString();
                  }
                  if (secondObj.HasMember("list") && secondObj["list"].IsArray()) {
                    for (SizeType k = 0; k < secondObj["list"].Size(); k++) {
                      if (secondObj["list"][k].IsString()) {
                        Tid10 tid10;
                        tid10.docid = secondObj["list"][k].GetString();
                        vtVideoSetDocId.push_back(secondObj["list"][k].GetString());
                        oSecond.docs.push_back(tid10);
                      }
                    }
                  }
                  oFirst.children.push_back(oSecond);
                }
              }
              vtVideoSetChildren.push_back(oFirst);
            }

            // 删除无文章节点
            vector<SKeyValue> vtValue;
            ret = DCacheGetStringBatch(vtVideoSetDocId, vtValue);
            map<string, string> mapDocInfo;
            for (size_t i = 0; i < vtValue.size(); i++) {
              if (vtValue.at(i).ret == 0) {
                string docidtemp  = vtValue.at(i).keyItem;
                string docinfo    = vtValue.at(i).value;
                mapDocInfo[docidtemp] = docinfo;
              }
            }
            for (vector<GetVideoSetFirst>::iterator iterFirst = vtVideoSetChildren.begin(); iterFirst != vtVideoSetChildren.end(); ) {
              for (vector<GetVideoSetSecond>::iterator iterSecond = (iterFirst->children).begin(); iterSecond != (iterFirst->children).end(); ) {
                for (vector<Tid10>::iterator iterDoc = (iterSecond->docs).begin(); iterDoc != (iterSecond->docs).end(); ) {
                  string docidtemp = iterDoc->docid;
                  if (!docidtemp.empty() && mapDocInfo.find(docidtemp) != mapDocInfo.end() && !mapDocInfo[docidtemp].empty()) {
                    ++iterDoc;
                  } else {
                    TLOGDEBUG("check docid not found, erase, docid: "<<docidtemp<<endl);
                    iterDoc = (iterSecond->docs).erase(iterDoc);
                  }
                }
                if ((iterSecond->docs).empty()) {
                  TLOGDEBUG("check docidlist empty, erase, title: "<<iterSecond->title<<endl);
                  iterSecond  = (iterFirst->children).erase(iterSecond);
                } else {
                  ++iterSecond;
                }
              }
              if ((iterFirst->children).empty()) {
                TLOGDEBUG("check second empty, erase, title: "<<iterFirst->title<<endl);
                iterFirst = vtVideoSetChildren.erase(iterFirst);
              } else {
                ++iterFirst;
              }
            }

            bool bVideoSetFlag = false;
            for (vector<GetVideoSetFirst>::const_iterator iterFirst = vtVideoSetChildren.begin(); iterFirst != vtVideoSetChildren.end(); ++iterFirst) {
              for (vector<GetVideoSetSecond>::const_iterator iterSecond = (iterFirst->children).begin(); iterSecond != (iterFirst->children).end(); ++iterSecond) {
                for (vector<Tid10>::const_iterator iterDoc = (iterSecond->docs).begin(); iterDoc != (iterSecond->docs).end(); ++iterDoc) {
                  if ((iterDoc->docid).compare(docid) == 0) {
                    TLOGDEBUG("find docid, first title:"<<iterFirst->title<<", second title: "<<iterSecond->title<<endl);
                    bVideoSetFlag   = true;
                    resp.set.stage  = iterFirst->title;
                    resp.set.set    = iterSecond->title;

                    DocDataSet oDocDataSet;
                    oDocDataSet.stage = iterFirst->title;
                    for (vector<GetVideoSetSecond>::const_iterator iterSecondRelate = (iterFirst->children).begin();
                         iterSecondRelate != (iterFirst->children).end(); ++iterSecondRelate) {
                      if ((iterSecondRelate->title).compare(iterSecond->title) != 0) {
                        oDocDataSet.set = iterSecondRelate->title;
                        resp.relateset.push_back(oDocDataSet);
                      }
                    }

                    ++iterFirst;
                    if (iterFirst != vtVideoSetChildren.end()) {
                      resp.nextstage = iterFirst->title;
                    }

                    for (vector<Tid10>::const_iterator iterDocRelate = (iterSecond->docs).begin(); iterDocRelate != (iterSecond->docs).end(); ++iterDocRelate) {
                      vtVideoRelateDocId.push_back(iterDocRelate->docid);
                    }
                    break;
                  }
                }
                if (bVideoSetFlag) {
                  break;
                }
              }
              if (bVideoSetFlag) {
                break;
              }
            }
          }
        }

        if (vtVideoRelateDocId.empty()) {
          TLOGDEBUG("check vtVideoRelateDocId empty, find by query"<<endl);
          string videoquery = "";
          if (type == 0) {
            // 根据疾病Tag+名家之声Tag搜索视频
            if (!docData.tid17.diseases.empty() && !docData.tid17.diseases[0].name.empty()) {
              videoquery = "diseases:"+docData.tid17.diseases[0].name+" AND tags:名家之声 AND ctype:8";
            }
          } else if (type == 1) {
            // 根据疾病Tag+医生名Tag搜索视频
            // if (!docData.tid17.diseases.empty() && !docData.tid17.diseases[0].name.empty() && !docData.tid17.op_doctor.name.empty()) {
            //   videoquery = "diseases:"+docData.tid17.diseases[0].name+" AND tags:"+docData.tid17.op_doctor.name+" AND ctype:8";
            // }
            if (!docData.tid17.diseases.empty() && !docData.tid17.diseases[0].name.empty() && docData.tid17.op_doctor.drid != 0) {
              ostringstream ossDrid;
              ossDrid << docData.tid17.op_doctor.drid;
              videoquery = "diseases:"+docData.tid17.diseases[0].name+" AND review_id:"+ossDrid.str()+" AND ctype:8";
            }
          } else if (type == 2) {
            // 根据疾病Tag+医生所属医院名Tag搜索视频
            // if (!docData.tid17.op_doctor.hospital.empty()) {
            //   videoquery = "diseases:"+docData.tid17.diseases[0].name+" AND tags:"+docData.tid17.op_doctor.hospital+" AND ctype:8";
            // }
            if (docData.tid17.op_doctor.hpid != 0) {
              ostringstream ossHpid;
              ossHpid << docData.tid17.op_doctor.hpid;
              videoquery = "diseases:"+docData.tid17.diseases[0].name+" AND review_hp_id:revdr_"+ossHpid.str()+" AND ctype:8";
            }
          }
          TLOGDEBUG("videoquery: "<<videoquery<<endl);
          if (!videoquery.empty()) {
            // 过滤文章
            SAGetDocIdListByTagsResp oSAGetDocIdListByTagsResp;
            int ret = SAGetDocIdListByTagsApi(header, videoquery, 0, oSAGetDocIdListByTagsResp);
            if (   ret != 0 || oSAGetDocIdListByTagsResp.docids.empty()
                || find(oSAGetDocIdListByTagsResp.docids.begin(), oSAGetDocIdListByTagsResp.docids.end(), docid) == oSAGetDocIdListByTagsResp.docids.end() ) {
              TLOGDEBUG("check oSAGetDocIdListByTagsResp.docids empty or docid not in oSAGetDocIdListByTagsResp.docids"
                        <<", query: "<<videoquery<<", ret: "<<ret<<endl);
            } else {
              vector<string> vtDrDocIdPre;    // 文章在系列视频第5篇, 6-10篇文章
              bool bDocIdFlag = false;        // 判断前置/后置文章
              for (vector<string>::const_iterator iterVideoDocId = oSAGetDocIdListByTagsResp.docids.begin();
                   iterVideoDocId != oSAGetDocIdListByTagsResp.docids.end(); ++iterVideoDocId) {
                string tdrlistdocid = *iterVideoDocId;
                if (tdrlistdocid.compare(docid) == 0) {
                  bDocIdFlag = true;
                }
                if (!bDocIdFlag) {
                  vtDrDocIdPre.push_back(tdrlistdocid);
                } else {
                  vtVideoRelateDocId.push_back(tdrlistdocid);  // 文章在系列视频第5篇, 1-4篇文章
                }
              }
              for (vector<string>::const_iterator iter = vtDrDocIdPre.begin(); iter != vtDrDocIdPre.end(); ++iter) {
                vtVideoRelateDocId.push_back(*iter);
              }
            }
          }
        }
        if (!vtVideoRelateDocId.empty()) {
          for (vector<string>::const_iterator iter = vtVideoRelateDocId.begin(); iter != vtVideoRelateDocId.end(); ++iter) {
            vtIgnoreDoc.push_back(*iter);
          }
          Value tlist(kArrayType);
          Document dVideoRelateDocs;
          Document::AllocatorType& allocator = dVideoRelateDocs.GetAllocator();
          GetDocItemList(vtVideoRelateDocId, tlist, allocator);
          for(SizeType iTlist = 0; iTlist < tlist.Size(); iTlist++) {
            if (tlist[iTlist].HasMember("tid") && tlist[iTlist]["tid"].IsInt() ) {
              Tid_DocInfo oDocInfo;
              if (tlist[iTlist]["tid"].GetInt() == 4) {
                oDocInfo.tid = 4;
                ret = Json2Jce::Tid4(tlist[iTlist], oDocInfo.tid4);
                if (ret != 0) {
                  TLOGDEBUG("tid4 json2jce fail");
                  continue;
                }
              } else if (tlist[iTlist]["tid"].GetInt() == 10) {
                oDocInfo.tid = 10;
                ret = Json2Jce::Tid10(tlist[iTlist], oDocInfo.tid10);
                if (ret != 0) {
                  TLOGDEBUG("tid10 json2jce fail");
                  continue;
                }
              } else if (tlist[iTlist]["tid"].GetInt() == 25) {
                oDocInfo.tid = 25;
                ret = Json2Jce::Tid25(tlist[iTlist], oDocInfo.tid25);
                if (ret != 0) {
                  TLOGDEBUG("tid25 json2jce fail");
                  continue;
                }
              }
              resp.videorelated.push_back(oDocInfo);
            }
          }
        } else {
          TLOGDEBUG("check vtVideoRelateDocId empty"<<endl);
        }
      }

      // 临床招募文章不展示相关文章
      int iDocType = 0;
      ret = mDataProxyHelper->GetCtypeByDocId(docid, iDocType);
      if (ret != 0) {
        TLOGERROR("GetCtypeByDocId fail, ret: "<<ret<<endl);
        return TencareBaike::E_DB_ERROR;
      }
      if (iDocType != 101) {
        // 获取疾病相关文章
        ret = -1;
        vector<string> vtRelateDoc;
        if (iDocType == 16) {
          // 专栏文章
          TLOGDEBUGEXT("get vtRelateDoc by cache");
          if (dDocInfo.HasMember("relateddocid") && dDocInfo["relateddocid"].IsArray()) {
            for(SizeType i = 0; i < dDocInfo["relateddocid"].Size(); i++) {
              if (dDocInfo["relateddocid"][i].IsString()) {
                vtRelateDoc.push_back(dDocInfo["relateddocid"][i].GetString());
              }
            }
          }
        } else {
          try {
            // 获取路由信息
            // std::string err_msg;
            // QOSREQUEST oRouteReq;
            // if (mDocRelModId != 0 && mDocRelCmd != 0) {
            //   oRouteReq._modid    = mDocRelModId;
            //   oRouteReq._cmd      = mDocRelCmd;
            //   float tm_out        = 0.2;     //设置调用获取路由超时时间
            //   ret = ApiGetRoute(oRouteReq, tm_out, err_msg);
            // }
            // if (ret < 0) {
            //   TLOGERROREXT("DocRel ApiGetRoute failed, ret: "<<ret<<", err: "<<err_msg);
            // } else {
              // ostringstream ossIpPort;
              // ossIpPort<<"http://"<<oRouteReq._host_ip<<":"<<oRouteReq._host_port<<"/";
              // string sDocRelUrl = ossIpPort.str();
              // TLOGDEBUGEXT("DocRel ApiGetRoute succ, url: "<<sDocRelUrl);

              // struct timeval start, end;
              // gettimeofday(&start, NULL);
              // ostringstream ossuin;
              // ossuin << header.uin;
              // string sDocRelStr = "docid="+docid+"&uin="+ossuin.str();
              // TLOGDEBUGEXT("sDocRelStr: "<<sDocRelStr);
              // TC_HttpRequest request;
              // TC_HttpResponse response;
              // request.setPostRequest(sDocRelUrl, sDocRelStr);
              // request.setContentType("application/x-www-form-urlencoded");
              // ret = request.doRequest(response);
              // std::string content = response.getContent();
              // TLOGDEBUGEXT("content: "<<content<<", ret: "<<ret);

              // gettimeofday(&end, NULL);
              // int use_time = (int)((end.tv_sec-start.tv_sec)*1000+(end.tv_usec - start.tv_usec)/1000);
              // int iApiRouteResultUpdateRet = ApiRouteResultUpdate(oRouteReq, ret, use_time, err_msg);
              // if(iApiRouteResultUpdateRet < 0) {
              //   TLOGERROREXT("ApiRouteResultUpdate failed, ret: "<<iApiRouteResultUpdateRet<<", err: "<<err_msg);
              // }

              // Document dConetent;
              // dConetent.Parse(content.c_str());
              // if (!dConetent.HasParseError() && dConetent.HasMember("list") && dConetent["list"].IsArray()) {
              //   for (SizeType i = 0; i < dConetent["list"].Size() && vtRelateDoc.size() < 20; i++) {
              //     if (   dConetent["list"][i].IsObject() && dConetent["list"][i].HasMember("docid")
              //         && dConetent["list"][i]["docid"].IsString()) {
              //       string sDocidTemp = dConetent["list"][i]["docid"].GetString();
              //       vtRelateDoc.push_back(sDocidTemp);
              //     }
              //   }
              // }
            // }
            TencareBaike::DrcmReq oDrcmReq;
            oDrcmReq.request_id = header.traceid;
            oDrcmReq.channel_id = "200";
            oDrcmReq.doc_id = req.docid;
            oDrcmReq.user_id = UL2S(header.uin);
            oDrcmReq.doc_num = 10;
            oDrcmReq.offset = 0;
            TencareBaike::DrcmResp oDrcmResp;
            ret = GetRelatedDocsApiGrpc(header, oDrcmReq, &oDrcmResp);
            if (ret != 0) {
              TLOGERROREXT("GetRelatedDocsApi fail, ret: "<<ret);
            }
            for(int i = 0; i < oDrcmResp.docs.size(); i++){
              string str = oDrcmResp.docs[i].doc_id;
              vtRelateDoc.push_back(str);
            }
            if (vtRelateDoc.size() > 20) vtRelateDoc.resize(20);
          } catch (exception& e) {
            TLOGERROREXT("parse Exception: "<<e.what());
            // return E_UNKNOWN_ERROR;
          } catch (...) {
            TLOGERROREXT("Unknown error");
            // return E_UNKNOWN_ERROR;
          }
          if (ret != 0) {
            TLOGDEBUGEXT("docrel fail, ret: "<<ret);
            mDataProxyHelper->GetDocCorrelation(docid, vtIgnoreDoc, 20, vtRelateDoc);
          }
        }
        TLOGDEBUGEXT("vtRelateDoc.size: "<<vtRelateDoc.size());

        if (!vtRelateDoc.empty()) {
          if (version > 0) {
            map<string, DocInfoV2> mapDocInfoV2;
            ret = GetDocInfoV2Map(vtRelateDoc, mapDocInfoV2);
            for (vector<string>::const_iterator iter = vtRelateDoc.begin(); iter != vtRelateDoc.end(); ++iter) {
              if (mapDocInfoV2.find(*iter) != mapDocInfoV2.end()) {
                resp.relatedcommon.push_back(mapDocInfoV2[*iter]);
              } else {
                TLOGDEBUGEXT("check docid not found, ignore, docid: "<<*iter);
              }
            }
          } else {
            Value tlist(kArrayType);
            Document dDocs;
            Document::AllocatorType& allocator = dDocs.GetAllocator();
            GetDocItemList(vtRelateDoc, tlist, allocator);
            for(SizeType iTlist = 0; iTlist < tlist.Size(); iTlist++) {
              if (tlist[iTlist].HasMember("tid") && tlist[iTlist]["tid"].IsInt() ) {
                Tid_DocInfo oDocInfo;
                if (tlist[iTlist]["tid"].GetInt() == 4) {
                  oDocInfo.tid = 4;
                  ret = Json2Jce::Tid4(tlist[iTlist], oDocInfo.tid4);
                  if (ret != 0) {
                    TLOGDEBUG("tid4 json2jce fail");
                    continue;
                  }
                  oDocInfo.tid4.type = 2;
                } else if (tlist[iTlist]["tid"].GetInt() == 10) {
                  oDocInfo.tid = 10;
                  ret = Json2Jce::Tid10(tlist[iTlist], oDocInfo.tid10);
                  if (ret != 0) {
                    TLOGDEBUG("tid10 json2jce fail");
                    continue;
                  }
                  oDocInfo.tid25.type = 2;
                } else if (tlist[iTlist]["tid"].GetInt() == 25) {
                  oDocInfo.tid = 25;
                  ret = Json2Jce::Tid25(tlist[iTlist], oDocInfo.tid25);
                  if (ret != 0) {
                    TLOGDEBUG("tid25 json2jce fail");
                    continue;
                  }
                  oDocInfo.tid25.type = 2;
                }
                resp.related.push_back(oDocInfo);
              }
            }
          }
        }
      } else if (101 == iDocType) {
        /*
           优先级 推荐逻辑
           1 同一个药品下的招募（药品使用招募中手工标签，除去XXX癌/临床招募）
           2 同一种治疗方式的招募（招募页的治疗方式字段）
           3 同一个疾病的招募（手工标签中的XXX癌）

           招募页无热门阅读
           拉取按之前，最多20篇
           */
        vector<string> vecDiseaseName;
        for (size_t level=0,iQueryCount=0; level < 3 && iQueryCount < 10; level++,iQueryCount++) {
          string strQueryKeyword;
          if (0 == level) {
            const string strPostfix("癌");
            // 获取标签信息
            if (dDocInfo.HasMember("tags") && dDocInfo["tags"].IsArray()) {
              for(SizeType iTag = 0; iTag < dDocInfo["tags"].Size(); iTag++) {
                const string &strTag = dDocInfo["tags"][iTag]["name"].GetString();
                TLOGDEBUG("101TAGS:"<<strTag<<endl);
                if (strTag.size() >= strPostfix.size() && strPostfix == strTag.substr(strTag.size()-strPostfix.size())) {
                  vecDiseaseName.push_back(strTag);
                  continue;
                }
                else if ("临床招募" == strTag || "内容" == strTag) {
                  continue;
                }
                strQueryKeyword = "tags:"+strTag;
              }
            }
          }
          else if (1 == level) {
            if (vtTreatment.empty()) continue;
            strQueryKeyword = "tags:"+vtTreatment.back();
            vtTreatment.pop_back();
          }
          else if (2 == level) {
            if (vecDiseaseName.empty()) continue;
            strQueryKeyword = "tags:"+vecDiseaseName.back();
            vecDiseaseName.pop_back();
          }
          TLOGDEBUG("["<<__FUNCTION__<<":"<<__LINE__<<"]|101LEVEL:"<<level<<", strQueryKeyword:"<<strQueryKeyword<<endl);
          if (strQueryKeyword.empty()) continue;

          // 过滤文章
          vector<string> vtRelateKeyItem;
          SAGetDocIdListByTagsResp oSAGetDocIdListByTagsResp;
          ret = SAGetDocIdListByTagsApi(header, strQueryKeyword, 101, oSAGetDocIdListByTagsResp);
          if (ret != 0 || oSAGetDocIdListByTagsResp.docids.empty()) {
            TLOGDEBUG("check oSAGetDocIdListByTagsResp.docids empty, continue"<<endl);
            continue;
          }
          for (vector<string>::const_iterator iterQueryKeyword = oSAGetDocIdListByTagsResp.docids.begin();
               iterQueryKeyword != oSAGetDocIdListByTagsResp.docids.end(); ++iterQueryKeyword) {
            string sRecruitListDocId = *iterQueryKeyword;
            if (sRecruitListDocId != docid && vtRelateKeyItem.size() < 20) {
              vtRelateKeyItem.push_back(sRecruitListDocId);
            }
          }

          if (!vtRelateKeyItem.empty()) {
            if (version > 0) {
              map<string, DocInfoV2> mapDocInfoV2;
              ret = GetDocInfoV2Map(vtRelateKeyItem, mapDocInfoV2);
              for (vector<string>::const_iterator iter = vtRelateKeyItem.begin(); iter != vtRelateKeyItem.end(); ++iter) {
                if (mapDocInfoV2.find(*iter) != mapDocInfoV2.end()) {
                  resp.relatedcommon.push_back(mapDocInfoV2[*iter]);
                } else {
                  TLOGDEBUGEXT("check docid not found, ignore, docid: "<<*iter);
                }
              }
            } else {
              Value tlist(kArrayType);
              Document dDocs;
              Document::AllocatorType& allocator = dDocs.GetAllocator();
              GetDocItemList(vtRelateKeyItem, tlist, allocator);
              for(SizeType iTlist = 0; iTlist < tlist.Size(); iTlist++) {
                if (tlist[iTlist].HasMember("tid") && tlist[iTlist]["tid"].IsInt() ) {
                  Tid_DocInfo oDocInfo;
                  if (tlist[iTlist]["tid"].GetInt() == 4) {
                    oDocInfo.tid = 4;
                    ret = Json2Jce::Tid4(tlist[iTlist], oDocInfo.tid4);
                    if (ret != 0) {
                      TLOGDEBUG("tid4 json2jce fail");
                      continue;
                    }
                    oDocInfo.tid4.type = 2;
                  } else if (tlist[iTlist]["tid"].GetInt() == 10) {
                    oDocInfo.tid = 10;
                    ret = Json2Jce::Tid10(tlist[iTlist], oDocInfo.tid10);
                    if (ret != 0) {
                      TLOGDEBUG("tid10 json2jce fail");
                      continue;
                    }
                    oDocInfo.tid25.type = 2;
                  } else if (tlist[iTlist]["tid"].GetInt() == 25) {
                    oDocInfo.tid = 25;
                    ret = Json2Jce::Tid25(tlist[iTlist], oDocInfo.tid25);
                    if (ret != 0) {
                      TLOGDEBUG("tid25 json2jce fail");
                      continue;
                    }
                    oDocInfo.tid25.type = 2;
                  }
                  resp.related.push_back(oDocInfo);
                }
              }
            }
          }
          TLOGDEBUGEXT("level: "<<level<<", vtRelateKeyItem.size: "<<vtRelateKeyItem.size()<<", resp.related.size: "<<resp.related.size());
          if (!resp.related.empty()) break;
          else if (1 == level && !vtTreatment.empty()) level--;
          else if (2 == level && !vecDiseaseName.empty()) level--;
        }
      } else {
        TLOGDEBUG("video doc ignore relatedoc"<<endl);
      }

      // 文章&问答详情页 相关标签
      if (   !resp.docdiseases.empty()
          && (1 == iDocType || 11 == iDocType) ) {
        map<string, int> mapTagNum;
        ret = mDataProxyHelper->GetTagCtypeNumByTagList(iDocType, resp.docdiseases[0].name, vtRelateTag, mapTagNum);
        if (ret != 0) {
          TLOGERROR("GetTagNumByTagList fail, ret: "<<ret<<endl);
          return TencareBaike::E_DB_ERROR;
        }
        for (vector<string>::const_iterator iterTag = vtRelateTag.begin();
             iterTag != vtRelateTag.end()/* && resp.relatetags.size() < 3*/; ++iterTag) {
          if (mapTagNum[*iterTag] > 1) {
            resp.relatetags.push_back(*iterTag);
          }
        }
      }

      // 获取疾病文章数
      if (!resp.docdiseases.empty()) {
        int iDocNum = 0;
        if (resp.docdiseases[0].name == "急救") {
          ret = mDataProxyHelper->GetTagNumByTag("all", resp.docdiseases[0].name, iDocNum);
        } else {
          ret = mDataProxyHelper->GetTagNumByTag(resp.docdiseases[0].name, resp.docdiseases[0].name, iDocNum);
        }
        if (ret == 0) {
          resp.docnum = iDocNum;
        }
      }
    }

    // 获取圈子信息
    std::string active_conmmunity_name = "圈子_圈子详情配置";
    std::string conmmunity_info_str;
    Document dCommunityInfo;
    ret = DCacheGetString(active_conmmunity_name, conmmunity_info_str);
    if (ret != 0) {
      TLOGERROREXT("DCacheGetString fail, ret: "<<ret<<", key: "<<active_conmmunity_name);
    } else if (!dCommunityInfo.Parse(conmmunity_info_str.c_str()).HasParseError() && dCommunityInfo.IsArray()) {
      for(SizeType iBaseInfo = 0; iBaseInfo < dCommunityInfo.Size(); iBaseInfo++) {
        if (dCommunityInfo[iBaseInfo].HasMember("community_id") && dCommunityInfo[iBaseInfo]["community_id"].IsInt()
          && dCommunityInfo[iBaseInfo]["community_id"].GetInt() != 0
          && dCommunityInfo[iBaseInfo].HasMember("pic") && dCommunityInfo[iBaseInfo]["pic"].IsString()
          && dCommunityInfo[iBaseInfo].HasMember("share_pic") && dCommunityInfo[iBaseInfo]["share_pic"].IsString()
          && dCommunityInfo[iBaseInfo].HasMember("key_words") && dCommunityInfo[iBaseInfo]["key_words"].IsArray()
          && dCommunityInfo[iBaseInfo].HasMember("diseases") && dCommunityInfo[iBaseInfo]["diseases"].IsArray()) {
          std::set<string> commuinity_tags;
          for(SizeType iKw = 0; iKw < dCommunityInfo[iBaseInfo]["key_words"].Size(); iKw++) {
            if (dCommunityInfo[iBaseInfo]["key_words"][iKw].IsString()) {
              commuinity_tags.insert(dCommunityInfo[iBaseInfo]["key_words"][iKw].GetString());
            }
          }
          for(SizeType iD = 0; iD < dCommunityInfo[iBaseInfo]["diseases"].Size(); iD++) {
            if (dCommunityInfo[iBaseInfo]["diseases"][iD].IsString()) {
              commuinity_tags.insert(dCommunityInfo[iBaseInfo]["diseases"][iD].GetString());
            }
          }
          int cid = 0;
          for (size_t i=0; i < vtDocDiseases.size(); i++) {
            if (0 != commuinity_tags.count(vtDocDiseases[i])) {
              cid = dCommunityInfo[iBaseInfo]["community_id"].GetInt();
              break;
            }
          }
          if (0 == cid) {
            for (size_t i=0; i < vtTag.size(); i++) {
              if (0 != commuinity_tags.count(vtTag[i])) {
                cid = dCommunityInfo[iBaseInfo]["community_id"].GetInt();
                break;
              }
            }
          }
          if (0 != cid) {
            if (header.uin > 0) {
              // 获取用户关注的圈子
              TencareBaike::GetMyCommunityListReq myCommunityListReq;
              TencareBaike::GetMyCommunityListResp myCommunityListResp;
              ret = GetMyCommunityListGrpc(header, myCommunityListReq, &myCommunityListResp);
              if (ret != 0) {
                TLOGERROREXT("GetMyCommunityList fail, ret: "<<ret);
              }
              if (myCommunityListResp.error_code == 0 && myCommunityListResp.community_list.size() > 0) {
                bool bCidCollect = false;
                for (size_t i = 0; i < myCommunityListResp.community_list.size(); ++i) {
                  if (cid == myCommunityListResp.community_list.at(i).cid) {
                    bCidCollect = true;
                    break;
                  }
                }
                if (bCidCollect) {
                  break;
                }
              }
            }

            try {
              TencareBaike::ReadKeyReq readKeyReq;
              TencareBaike::ReadKeyRsp readKeyRsp;
              readKeyReq.k = mDataproxycPrefix + "community_info_" + I2S(cid);
              ret = mDataProxyObjPrx->ReadKey(readKeyReq, readKeyRsp);
              TLOGDEBUGEXT("ReadKey key:"<<readKeyReq.k<<", ret:"<<ret<<", rsp:"<<readKeyRsp.writeToJsonString());
              if (0 == ret && 0 == readKeyRsp.retcode && !readKeyRsp.val.empty()) {
                Document dCommunityBaseInfo;
                if (!dCommunityBaseInfo.Parse(readKeyRsp.val.c_str()).HasParseError()
                  && dCommunityBaseInfo.HasMember("name") && dCommunityBaseInfo["name"].IsString()) {
                  resp.communityInfo.cid = cid;
                  resp.communityInfo.name = dCommunityBaseInfo["name"].GetString();
                  resp.communityInfo.pic = dCommunityInfo[iBaseInfo]["pic"].GetString();
                  resp.communityInfo.share_pic = dCommunityInfo[iBaseInfo]["share_pic"].GetString();
                  break;
                }
              }
            } catch (exception& e) {
              TLOGERROR("ReadKey Exception: "<<e.what()<<endl);
            } catch (...) {
              TLOGERROR("Unknown error");
            }
          }
        }
      }
    } else {
      TLOGERROREXT("conmmunity_info_str value error, conmmunity_info_str: "<<conmmunity_info_str);
    }
  } while(0);

  // 标签树节点信息(标签树V5)
  GetOverviewTreeV5NoteTagInfoResp oGetOverviewTreeV5NoteTagInfoResp;
  if (!treerootid.empty()) {
    string sDiseaseTemp = "";
    if (!vtDocDiseases.empty()) {
      sDiseaseTemp = vtDocDiseases[0];
    }
    GetOverviewTreeV5NoteTagInfoApi(header, docid, treerootid, sDiseaseTemp, oGetOverviewTreeV5NoteTagInfoResp);
  } else {
    for (vector<string>::const_iterator iter = vtDocDiseases.begin(); iter != vtDocDiseases.end(); ++iter) {
      GetOverviewTreeV5NoteTagInfoApi(header, docid, "", *iter, oGetOverviewTreeV5NoteTagInfoResp);
      if (!oGetOverviewTreeV5NoteTagInfoResp.notetaginfo.empty()) {
        TLOGDEBUG("find notetaginfo, break"<<endl);
        break;
      }
    }
  }
  for (vector<NodeTagInfo>::const_iterator iter = oGetOverviewTreeV5NoteTagInfoResp.notetaginfo.begin();
       iter != oGetOverviewTreeV5NoteTagInfoResp.notetaginfo.end(); ++iter) {
    resp.notetaginfo.push_back(*iter);
  }

  // 问卷标志位
  GetActiveDataResp oGetActiveDataResp;
  ret = GetActiveDataApi(header, ACTIVE_TYPE_DOCDATA_QUEOPTV2, "", oGetActiveDataResp);
  if (ret != 0) {
    TLOGERROR("GetActiveDataApi fail, ret: "<<ret<<", activetype: "<<ACTIVE_TYPE_DOCDATA_QUEOPTV2<<endl);
  } else {
    if (!oGetActiveDataResp.text.empty() && !oGetActiveDataResp.activelist.empty()) {
      GetDocsDataByTagIdResp oGetDocsDataByTagIdResp;
      ret = GetDocsDataByTagIdApi(header, oGetActiveDataResp.text, 1, oGetDocsDataByTagIdResp);
      if (ret != 0) {
        TLOGERROR("GetDocsDataByTagIdApi fail, ret: "<<ret<<", name: "<<oGetActiveDataResp.text<<", docidlistflag: 1"<<endl);
      } else {
        if (find(oGetDocsDataByTagIdResp.docidlist.begin(), oGetDocsDataByTagIdResp.docidlist.end(), docid) != oGetDocsDataByTagIdResp.docidlist.end()) {
          DCQueryDocDataQueOptItemByKeyReq oDCQueryDocDataQueOptItemByKeyReq;
          oDCQueryDocDataQueOptItemByKeyReq.docid = docid;
          oDCQueryDocDataQueOptItemByKeyReq.uin   = header.uin;
          DCQueryDocDataQueOptItemByKeyResp oDCQueryDocDataQueOptItemByKeyResp;
          ret = mDataProxyHelper->QueryDocDataQueOptItemByKey(oDCQueryDocDataQueOptItemByKeyReq.docid, oDCQueryDocDataQueOptItemByKeyReq.uin, oDCQueryDocDataQueOptItemByKeyResp);
          if (ret == -1) {
            resp.queopt.id = oGetActiveDataResp.docnum;
            for (vector<ActiveItem>::const_iterator iter = oGetActiveDataResp.activelist.begin();
                 iter != oGetActiveDataResp.activelist.end(); ++iter) {
              GetDocDataRelatedQueOptData oGetDocDataRelatedQueOptData;
              oGetDocDataRelatedQueOptData.name   = iter->topic;
              oGetDocDataRelatedQueOptData.title  = iter->text;
              oGetDocDataRelatedQueOptData.type   = iter->type;
              for (vector<string>::const_iterator iterQueopt = (iter->titles).begin();
                   iterQueopt != (iter->titles).end(); ++iterQueopt) {
                oGetDocDataRelatedQueOptData.queopt.push_back(*iterQueopt);
              }
              resp.queopt.data.push_back(oGetDocDataRelatedQueOptData);
            }
            TLOGDEBUG("DCQueryDocDataQueOptItemByKey not found, docid: "<<oDCQueryDocDataQueOptItemByKeyReq.docid
                      <<", uin: "<<oDCQueryDocDataQueOptItemByKeyReq.uin<<endl);
          } else if (ret != 0) {
            TLOGDEBUG("DCQueryDocDataQueOptItemByKey fail, ret: "<<ret<<endl);
          }
        } else {
          TLOGDEBUG("docid not found in oGetDocsDataByTagIdResp.docidlist"<<endl);
        }
      }
    } else {
      TLOGDEBUG("check oGetActiveDataResp.text empty"<<endl);
    }
  }

  // 肿瘤页疾病目录树V5文章
  if (!treerootid.empty() && !treeid.empty()) {
    string sOverviewTreeV5DocId;
    GetOverviewTreeV5NextRecommendDocResp oGetOverviewTreeV5NextRecommendDocResp;
    GetOverviewTreeV5NextRecommendDocApi(header, treerootid, treeid, oGetOverviewTreeV5NextRecommendDocResp);
    sOverviewTreeV5DocId = oGetOverviewTreeV5NextRecommendDocResp.docid;
    if (!sOverviewTreeV5DocId.empty()) {
      string overviewtreev5doc_str;
      ret = DCacheGetString(sOverviewTreeV5DocId, overviewtreev5doc_str);
      if (ret != 0) {
        TLOGERROR("DCacheGetString fail, ret: "<<ret<<", key: "<<sOverviewTreeV5DocId<<endl);
      } else {
        TLOGINFO("docid: "<<sOverviewTreeV5DocId<<", docinfo: "<<overviewtreev5doc_str<<endl);
        Document dOverviewTreeV5DocInfo;
        if (dOverviewTreeV5DocInfo.Parse(overviewtreev5doc_str.c_str()).HasParseError()) {
          TLOGDEBUG("parse overviewtreev5doc_str fail: "<<overviewtreev5doc_str<<endl);
        } else {
          if (dOverviewTreeV5DocInfo.HasMember("index") && dOverviewTreeV5DocInfo["index"].HasMember("tid")) {
            // 获取标签信息
            Value& vDocData = dOverviewTreeV5DocInfo["index"];
            int tid = vDocData["tid"].GetInt();
            if (tid == 4) {
              Tid4 tid4;
              Json2Jce::Tid4(vDocData, tid4);
              resp.overviewtreev5doc.title = tid4.title;
            } else if (tid == 10) {
              Tid10 tid10;
              Json2Jce::Tid10(vDocData, tid10);
              resp.overviewtreev5doc.title = tid10.title;
            } else if (tid == 25) {
              Tid25 tid25;
              Json2Jce::Tid25(vDocData, tid25);
              resp.overviewtreev5doc.title = tid25.title;
            }
            resp.overviewtreev5doc.docid    = sOverviewTreeV5DocId;
            resp.overviewtreev5doc.treeid   = oGetOverviewTreeV5NextRecommendDocResp.nexttreeid;
          }
        }
      }
    }
  }

  // 专题文章
  if (!tagid.empty()) {
    GetDocsDataByTagIdResp oGetDocsDataByTagIdResp;
    ret = GetDocsDataByTagIdApi(header, tagid, 1, oGetDocsDataByTagIdResp);
    if (ret != 0) {
      TLOGERROR("GetDocsDataByTagIdApi fail, ret: "<<ret<<", name: "<<tagid<<", docidlistflag: 1"<<endl);
    } else {
      vector<string>::iterator iterFind = find(oGetDocsDataByTagIdResp.docidlist.begin(), oGetDocsDataByTagIdResp.docidlist.end(), docid);
      if (iterFind != oGetDocsDataByTagIdResp.docidlist.end()) {
        ++iterFind;
        if (iterFind != oGetDocsDataByTagIdResp.docidlist.end()) {
          string sTagTreeDocId = *iterFind;
          if (!sTagTreeDocId.empty()) {
            string tagtreedoc_str;
            ret = DCacheGetString(sTagTreeDocId, tagtreedoc_str);
            if (ret != 0) {
              TLOGERROR("DCacheGetString fail, ret: "<<ret<<", key: "<<sTagTreeDocId<<endl);
            } else {
              TLOGINFO("docid: "<<sTagTreeDocId<<", docinfo: "<<tagtreedoc_str<<endl);
              Document dTagTreeDocInfo;
              if (dTagTreeDocInfo.Parse(tagtreedoc_str.c_str()).HasParseError()) {
                TLOGDEBUG("parse tagtreedoc_str fail: "<<tagtreedoc_str<<endl);
              } else {
                if (dTagTreeDocInfo.HasMember("index") && dTagTreeDocInfo["index"].HasMember("tid")) {
                  // 获取标签信息
                  Value& vDocData = dTagTreeDocInfo["index"];
                  int tid = vDocData["tid"].GetInt();
                  GetDocDataRelatedOverviewTreeV5Doc oGetDocDataRelatedOverviewTreeV5Doc;
                  if (tid == 4) {
                    Tid4 tid4;
                    Json2Jce::Tid4(vDocData, tid4);
                    oGetDocDataRelatedOverviewTreeV5Doc.title = tid4.title;
                    oGetDocDataRelatedOverviewTreeV5Doc.image = tid4.op_image;
                  } else if (tid == 10) {
                    Tid10 tid10;
                    Json2Jce::Tid10(vDocData, tid10);
                    oGetDocDataRelatedOverviewTreeV5Doc.title = tid10.title;
                    oGetDocDataRelatedOverviewTreeV5Doc.image = tid10.image;
                  } else if (tid == 25) {
                    Tid25 tid25;
                    Json2Jce::Tid25(vDocData, tid25);
                    oGetDocDataRelatedOverviewTreeV5Doc.title = tid25.title;
                    oGetDocDataRelatedOverviewTreeV5Doc.image = tid25.image;
                  }
                  oGetDocDataRelatedOverviewTreeV5Doc.docid    = sTagTreeDocId;
                  oGetDocDataRelatedOverviewTreeV5Doc.treeid   = tagid;

                  if (!oGetDocDataRelatedOverviewTreeV5Doc.title.empty()) {
                    resp.tagtreedoc.push_back(oGetDocDataRelatedOverviewTreeV5Doc);
                  }
                }
              }
            }
          }
        } else {
          TLOGDEBUG("docid-next not found in oGetDocsDataByTagIdResp.docidlist, docid: "<<docid<<endl);
        }
      } else {
        TLOGDEBUG("docid not found in oGetDocsDataByTagIdResp.docidlist, docid: "<<docid<<endl);
      }
    }
  }

  // 获取所属疾病相关疾病/症状/药品
  // step 1 找到所属疾病docid
  std::vector<SDcacheDiseaseInfo> oInfos;
  std::map<std::string, std::vector<SDcacheDiseaseInfo> > disesae_onInfos;
  GetDiseaseInfoByDiseaseItems(vtDocDiseases, &oInfos, &disesae_onInfos);
  TLOGDEBUGEXT("vtDocDiseases_size=" << vtDocDiseases.size() << ",disesae_onInfos_size=" << disesae_onInfos.size());
  for (size_t i = 0; i < oInfos.size(); ++i) {
    TLOGDEBUGEXT("did:" << oInfos[i].did << ", docid=" << oInfos[i].docid);
  }

  std::vector<std::string> vtDocid;
  for (std::vector<SDcacheDiseaseInfo>::iterator it = oInfos.begin();
       it != oInfos.end(); ++it) {
    vtDocid.push_back(it->docid);
  }

  // step 2 根据docid找到相关疾病/病症/药品
  std::vector<std::string> related_diseases;
  std::vector<std::string> related_docids;
  std::set<std::string> related_docid_set;
  vector<SKeyValue> vtValue;
  DCacheGetStringBatch(vtDocid, vtValue);

  disesae_onInfos.clear();
  for (size_t i = 0; i < vtValue.size(); i++) {
    if (vtValue.at(i).ret == 0) {
      string docid  = vtValue.at(i).keyItem;
      string docinfo = vtValue.at(i).value;
      Document dDocInfo;
      // TLOGDEBUGEXT("docid:" << docid << ", docinfo:" << docinfo);
      if (!dDocInfo.Parse(docinfo.c_str()).HasParseError()) {
        if (dDocInfo.HasMember("related_disease") && dDocInfo["related_disease"].IsArray()) {
          for (size_t j = 0; j < dDocInfo["related_disease"].Size(); ++j) {
            Value& related_disease = dDocInfo["related_disease"][j];

            if (related_disease.HasMember("name") && related_disease["name"].IsString() &&
                related_disease.HasMember("docid") && related_disease["docid"].IsString()) {
              std::string name = related_disease["name"].GetString();
              related_diseases.push_back(name);
              SDcacheDiseaseInfo dinfo;
              dinfo.docid = related_disease["docid"].GetString();
              if (related_docid_set.find(dinfo.docid) != related_docid_set.end()) continue;

              disesae_onInfos[name].push_back(dinfo);
              related_docids.push_back(dinfo.docid);
              related_docid_set.insert(dinfo.docid);
            }
          }
        }
      }
    }
  }

  TLOGDEBUGEXT("related_docids_size:" << related_docids.size());

  // step 3 获取ctype 和single_abs
  std::map<std::string, DocidInfo> docid_docidinfo;
  GetDocidInfos(related_docids, &docid_docidinfo);
  for (std::map<std::string, DocidInfo>::iterator it = docid_docidinfo.begin(); it != docid_docidinfo.end();
       ++it) {
    TLOGDEBUGEXT("docid:" << it->first << ", DocidInfo=" << it->second.Dump());
  }

  std::map<std::string, std::string> docid_single_abs;
  vtValue.clear();
  vector<std::string> vtKey;
  DCacheGetStringBatch(related_docids, vtValue);
  map<string, string> mapDocInfo;
  for (size_t i = 0; i < vtValue.size(); i++) {
    if (vtValue.at(i).ret == 0) {
      string docid  = vtValue.at(i).keyItem;
      string docs_str = vtValue.at(i).value;

      Document dDocInfo;
      dDocInfo.Parse(docs_str.c_str());
      if (dDocInfo.HasParseError()) {
        TLOGERROREXT("parse related_doc_str fail: "<< docs_str <<endl);
        continue;
      }

      if (dDocInfo.HasMember("single_abs") && dDocInfo["single_abs"].IsString()) {
        docid_single_abs[docid] = dDocInfo["single_abs"].GetString();
      }
    }
  }

  for (std::map<std::string, std::string>::iterator it = docid_single_abs.begin(); it != docid_single_abs.end();
       ++it) {
    TLOGDEBUGEXT("docid:" << it->first << ", single_abs=" << it->second);
  }

  // step 4 输出结果
  map<string, DiseaseItem> mapDiseaseInfoList;
  GetDiseaseInfoList(related_diseases, mapDiseaseInfoList);
  std::vector<int> all_ctype;
  all_ctype.push_back(0);
  all_ctype.push_back(10);
  all_ctype.push_back(20);
  for (size_t j = 0; j < all_ctype.size(); j++) {
    for (size_t i = 0; i < related_diseases.size(); i ++) {
      std::string& disease = related_diseases[i];
      std::map<std::string, std::vector<SDcacheDiseaseInfo> >::iterator it = disesae_onInfos.find(disease);
      if (it == disesae_onInfos.end()) {
        TLOGERROREXT("missing_disease=" << disease);
        continue;
      }
      for (std::vector<SDcacheDiseaseInfo>::iterator dit = it->second.begin();
           dit != it->second.end(); ++dit) {
        std::string docid = dit->docid;
        std::map<std::string, DocidInfo>::iterator docidinfo_it = docid_docidinfo.find(docid);
        if (docidinfo_it == docid_docidinfo.end()) continue;

        if (docidinfo_it->second.ctype == all_ctype[j]) {
          RelateKnowledge related_knowledge;
          related_knowledge.title     = disease;
          related_knowledge.abs       = docid_single_abs[docid];
          related_knowledge.docid = docid;
          if (mapDiseaseInfoList.find(disease) != mapDiseaseInfoList.end()) {
            related_knowledge.type      = mapDiseaseInfoList[disease].type;
            related_knowledge.released  = mapDiseaseInfoList[disease].released;
            related_knowledge.h5url     = mapDiseaseInfoList[disease].h5url;
            related_knowledge.wxaurl    = mapDiseaseInfoList[disease].wxaurl;
          }
          resp.related_knowledges.push_back(related_knowledge);
        }
      }
    }
  }

  TLOGDEBUG("knowledge_size=" << resp.related_knowledges.size()<<endl);
  for (size_t i = 0; i < resp.related_knowledges.size(); ++i) {
    TLOGDEBUG("knowledge[" << i << "]=" << resp.related_knowledges[i].writeToJsonString());
  }

  // 获取疾病全流程方案文章信息
  if (!disdocid.empty() && !dislisttabid.empty()) {
    BatchGetDiseaseListTabDataReq oBatchGetDiseaseListTabDataReq;
    oBatchGetDiseaseListTabDataReq.id = disdocid;
    BatchGetDiseaseListTabDataResp oBatchGetDiseaseListTabDataResp;
    ret = BatchGetDiseaseListTabData(header, oBatchGetDiseaseListTabDataReq, oBatchGetDiseaseListTabDataResp, current);
    if (ret != 0) {
      TLOGDEBUGEXT("BatchGetDiseaseListTabData fail, ret: "<<ret<<", id: "<<oBatchGetDiseaseListTabDataReq.id);
    } else {
      vector<pair<string, GetDiseaseListDocs> > vtTagIdDiseaseListDocs;   // 标签ID-文章信息
      for (vector<DiseaseListTabDataItem>::const_iterator iter = oBatchGetDiseaseListTabDataResp.list.begin();
           iter != oBatchGetDiseaseListTabDataResp.list.end(); ++iter) {
        for (vector<DiseaseListTabDataSecond>::const_iterator iterSecond = (iter->list).begin();
             iterSecond != (iter->list).end(); ++iterSecond) {
          for (vector<DocInfoV2>::const_iterator iterDocs = (iterSecond->docs).begin();
               iterDocs != (iterSecond->docs).end(); ++iterDocs) {
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid  = iterDocs->docid;
            oGetDiseaseListDocs.title  = iterDocs->title;
            oGetDiseaseListDocs.tab    = iter->name;
            oGetDiseaseListDocs.id     = iterSecond->id;
            oGetDiseaseListDocs.h5url  = iterDocs->h5url;
            oGetDiseaseListDocs.wxaurl = iterDocs->wxaurl;
            vtTagIdDiseaseListDocs.push_back(make_pair(iterSecond->id, oGetDiseaseListDocs));
          }
        }
        for (vector<DocInfoV2>::const_iterator iterDocs = (iter->docs).begin();
             iterDocs != (iter->docs).end(); ++iterDocs) {
          GetDiseaseListDocs oGetDiseaseListDocs;
          oGetDiseaseListDocs.docid  = iterDocs->docid;
          oGetDiseaseListDocs.title  = iterDocs->title;
          oGetDiseaseListDocs.tab    = iter->name;
          oGetDiseaseListDocs.id     = iter->id;
          oGetDiseaseListDocs.h5url  = iterDocs->h5url;
          oGetDiseaseListDocs.wxaurl = iterDocs->wxaurl;
          vtTagIdDiseaseListDocs.push_back(make_pair(iter->id, oGetDiseaseListDocs));
        }
      }
      TLOGDEBUGEXT("vtTagIdDiseaseListDocs.size: "<<vtTagIdDiseaseListDocs.size());

      for (size_t i = 0; i < vtTagIdDiseaseListDocs.size(); ++i) {
        if (vtTagIdDiseaseListDocs[i].first.compare(dislisttabid) == 0 &&
            vtTagIdDiseaseListDocs[i].second.docid.compare(docid) == 0) {
          TLOGDEBUGEXT("vtTagIdDiseaseListDocs match docid, i: "<<i);
          // 前一篇文章数据
          if (i == 0) {
            // 如果当前是第一篇, 则前一篇为最后一篇
            resp.dislistrel.push_back(vtTagIdDiseaseListDocs[vtTagIdDiseaseListDocs.size()-1].second);
          } else {
            resp.dislistrel.push_back(vtTagIdDiseaseListDocs[i-1].second);
          }
          // 后一篇文章数据
          if (i == vtTagIdDiseaseListDocs.size()-1) {
            // 如果当前是最后一篇, 则后一篇为第一篇
            resp.dislistrel.push_back(vtTagIdDiseaseListDocs[0].second);
          } else {
            resp.dislistrel.push_back(vtTagIdDiseaseListDocs[i+1].second);
          }
        }
      }
    }
  }

  // 获取目录索引结构方案文章信息
  if (!disdocid.empty() && !discatalogtabid.empty()) {
    GetDiseaseCatalogTabDataReq oGetDiseaseCatalogTabDataReq;
    oGetDiseaseCatalogTabDataReq.id = disdocid;
    GetDiseaseCatalogTabDataResp oGetDiseaseCatalogTabDataResp;
    ret = GetDiseaseCatalogTabData(header, oGetDiseaseCatalogTabDataReq, oGetDiseaseCatalogTabDataResp, current);
    if (ret != 0) {
      TLOGDEBUGEXT("GetDiseaseCatalogTabData fail, ret: "<<ret<<", id: "<<oGetDiseaseCatalogTabDataReq.id);
    } else {
      string sTabName, sTabName2;                                           // 一级/二级标签名
      vector<pair<string, GetDiseaseListDocs> > vtTagIdDiseaseCatalogDocs;  // 标签ID-文章信息
      for (vector<DiseaseCatalogTabDataItem>::const_iterator iter = oGetDiseaseCatalogTabDataResp.list.begin();
           iter != oGetDiseaseCatalogTabDataResp.list.end(); ++iter) {
        for (vector<DiseaseCatalogTabDataSecond>::const_iterator iterSecond = (iter->list).begin();
             iterSecond != (iter->list).end(); ++iterSecond) {
          for (vector<string>::const_iterator iterDocs = (iterSecond->docs).begin();
               iterDocs != (iterSecond->docs).end(); ++iterDocs) {
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid = *iterDocs;
            oGetDiseaseListDocs.tab   = iter->name;
            oGetDiseaseListDocs.id    = iterSecond->id;
            vtTagIdDiseaseCatalogDocs.push_back(make_pair(iterSecond->id, oGetDiseaseListDocs));
          }
          if ((iterSecond->id).compare(discatalogtabid) == 0) {
            sTabName  = iter->name;
            sTabName2 = iterSecond->name;
            TLOGDEBUGEXT("sTabName: "<<sTabName<<", sTabName2: "<<sTabName2);
          }
        }
      }
      TLOGDEBUGEXT("vtTagIdDiseaseCatalogDocs.size: "<<vtTagIdDiseaseCatalogDocs.size());

      vector<string> vtDocId;
      vector<pair<string, GetDiseaseListDocs> > vtPreLaterDocIdDocInfo;   // 前一篇/后一篇文章docid-doc信息
      vector<string> vtCatalogDocId;                                      // 目录文章
      for (size_t i = 0; i < vtTagIdDiseaseCatalogDocs.size(); ++i) {
        if (vtTagIdDiseaseCatalogDocs[i].first.compare(discatalogtabid) == 0 &&
            vtTagIdDiseaseCatalogDocs[i].second.docid.compare(docid) == 0) {
          TLOGDEBUGEXT("vtTagIdDiseaseCatalogDocs match docid, i: "<<i);
          // 前一篇文章数据
          if (i == 0) {
            // 如果当前是第一篇, 则前一篇为最后一篇
            vtDocId.push_back(vtTagIdDiseaseCatalogDocs[vtTagIdDiseaseCatalogDocs.size()-1].second.docid);
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid = vtTagIdDiseaseCatalogDocs[vtTagIdDiseaseCatalogDocs.size()-1].second.docid;
            oGetDiseaseListDocs.tab   = vtTagIdDiseaseCatalogDocs[vtTagIdDiseaseCatalogDocs.size()-1].second.tab;
            oGetDiseaseListDocs.id    = vtTagIdDiseaseCatalogDocs[vtTagIdDiseaseCatalogDocs.size()-1].second.id;
            vtPreLaterDocIdDocInfo.push_back(make_pair(oGetDiseaseListDocs.docid, oGetDiseaseListDocs));
          } else {
            vtDocId.push_back(vtTagIdDiseaseCatalogDocs[i-1].second.docid);
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid = vtTagIdDiseaseCatalogDocs[i-1].second.docid;
            oGetDiseaseListDocs.tab   = vtTagIdDiseaseCatalogDocs[i-1].second.tab;
            oGetDiseaseListDocs.id    = vtTagIdDiseaseCatalogDocs[i-1].second.id;
            vtPreLaterDocIdDocInfo.push_back(make_pair(oGetDiseaseListDocs.docid, oGetDiseaseListDocs));
          }
          // 后一篇文章数据
          if (i == vtTagIdDiseaseCatalogDocs.size()-1) {
            // 如果当前是最后一篇, 则后一篇为第一篇
            vtDocId.push_back(vtTagIdDiseaseCatalogDocs[0].second.docid);
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid = vtTagIdDiseaseCatalogDocs[0].second.docid;
            oGetDiseaseListDocs.tab   = vtTagIdDiseaseCatalogDocs[0].second.tab;
            oGetDiseaseListDocs.id    = vtTagIdDiseaseCatalogDocs[0].second.id;
            vtPreLaterDocIdDocInfo.push_back(make_pair(oGetDiseaseListDocs.docid, oGetDiseaseListDocs));
          } else {
            vtDocId.push_back(vtTagIdDiseaseCatalogDocs[i+1].second.docid);
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid = vtTagIdDiseaseCatalogDocs[i+1].second.docid;
            oGetDiseaseListDocs.tab   = vtTagIdDiseaseCatalogDocs[i+1].second.tab;
            oGetDiseaseListDocs.id    = vtTagIdDiseaseCatalogDocs[i+1].second.id;
            vtPreLaterDocIdDocInfo.push_back(make_pair(oGetDiseaseListDocs.docid, oGetDiseaseListDocs));
          }
        }
        if (vtTagIdDiseaseCatalogDocs[i].first.compare(discatalogtabid) == 0) {
          TLOGDEBUGEXT("vtTagIdDiseaseCatalogDocs match tabid, docid: "<<vtTagIdDiseaseCatalogDocs[i].second.docid);
          vtDocId.push_back(vtTagIdDiseaseCatalogDocs[i].second.docid);
          vtCatalogDocId.push_back(vtTagIdDiseaseCatalogDocs[i].second.docid);
        }
      }
      if (vtPreLaterDocIdDocInfo.empty() && !vtCatalogDocId.empty()) {
        // 前一篇后一篇没数据, 表示docid和discatalogtabid不匹配, 需要把vtCatalogDocId数据清空
        TLOGDEBUGEXT("check vtPreLaterDocIdDocInfo empty, clear vtCatalogDocId");
        vtCatalogDocId.clear();
      }
      TLOGDEBUGEXT("vtDocId.size: "<<vtDocId.size()<<", vtPreLaterDocIdDocInfo.size: "<<vtPreLaterDocIdDocInfo.size()<<", "<<
                   "vtCatalogDocId.size: "<<vtCatalogDocId.size());
      if (!vtDocId.empty()) {
        map<string, DocInfoV2> mapDocInfoV2;
        ret = GetDocInfoV2Map(vtDocId, mapDocInfoV2);
        // 前一篇/后一篇文章
        for (vector<pair<string, GetDiseaseListDocs> >::const_iterator iter = vtPreLaterDocIdDocInfo.begin();
             iter != vtPreLaterDocIdDocInfo.end(); ++iter) {
          if (mapDocInfoV2.find(iter->first) != mapDocInfoV2.end()) {
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid  = mapDocInfoV2[iter->first].docid;
            oGetDiseaseListDocs.title  = mapDocInfoV2[iter->first].title;
            oGetDiseaseListDocs.tab    = (iter->second).tab;
            oGetDiseaseListDocs.id     = (iter->second).id;
            oGetDiseaseListDocs.h5url  = mapDocInfoV2[iter->first].h5url;
            oGetDiseaseListDocs.wxaurl = mapDocInfoV2[iter->first].wxaurl;
            resp.dislistrel.push_back(oGetDiseaseListDocs);
          } else {
            TLOGDEBUGEXT("check docid not found, ignore, docid: "<<iter->first);
          }
        }
        // 目录文章
        GetDiseaseCatalogRelated oGetDiseaseCatalogRelated;
        oGetDiseaseCatalogRelated.tab   = sTabName;
        oGetDiseaseCatalogRelated.tab2  = sTabName2;
        for (vector<string>::const_iterator iter = vtCatalogDocId.begin(); iter != vtCatalogDocId.end(); ++iter) {
          if (mapDocInfoV2.find(*iter) != mapDocInfoV2.end()) {
            GetDiseaseListDocs oGetDiseaseListDocs;
            oGetDiseaseListDocs.docid  = mapDocInfoV2[*iter].docid;
            oGetDiseaseListDocs.title  = mapDocInfoV2[*iter].title;
            oGetDiseaseListDocs.h5url  = mapDocInfoV2[*iter].h5url;
            oGetDiseaseListDocs.wxaurl = mapDocInfoV2[*iter].wxaurl;
            oGetDiseaseCatalogRelated.docs.push_back(oGetDiseaseListDocs);
          } else {
            TLOGDEBUGEXT("check docid not found, ignore, docid: "<<*iter);
          }
        }
        if (!oGetDiseaseCatalogRelated.docs.empty()) {
          resp.discatalogrel.push_back(oGetDiseaseCatalogRelated);
        }
      }
    }
  }

  return TencareBaike::E_SUCCESS;
}
